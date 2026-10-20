#include "embedded_resources.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;

constexpr char kAutoLaunchFileName[] = "server__autoLaunch.sh";
constexpr char kPathFileName[] = "server__path.sh";
constexpr auto kBackgroundStopTimeout = std::chrono::seconds(2);
constexpr auto kForegroundStopTimeout = std::chrono::seconds(2);
constexpr auto kProcessPollInterval = std::chrono::milliseconds(20);

volatile std::sig_atomic_t gBackgroundPid = -1;
volatile std::sig_atomic_t gBackgroundHasProcessGroup = 0;
volatile std::sig_atomic_t gForegroundPid = -1;
volatile std::sig_atomic_t gTerminationRequested = 0;

struct BackgroundProcess {
    pid_t pid = -1;
    bool hasProcessGroup = false;
};

std::string systemError(const std::string& action) {
    return action + ": " + std::strerror(errno);
}

void forwardSignal(int signalNumber) {
    const int savedErrno = errno;
    const pid_t backgroundPid = static_cast<pid_t>(gBackgroundPid);
    const pid_t foregroundPid = static_cast<pid_t>(gForegroundPid);

    if (signalNumber != SIGINT) {
        gTerminationRequested = signalNumber;
    }

    if (backgroundPid > 0) {
        if (gBackgroundHasProcessGroup != 0) {
            (void)::kill(-backgroundPid, signalNumber);
        }
        (void)::kill(backgroundPid, signalNumber);
    }
    if (foregroundPid > 0) {
        (void)::kill(foregroundPid, signalNumber);
    }

    errno = savedErrno;
}

void installSignalHandlers() {
    struct sigaction action {};
    action.sa_handler = forwardSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;

    for (const int signalNumber : {SIGINT, SIGTERM, SIGHUP, SIGQUIT}) {
        if (::sigaction(signalNumber, &action, nullptr) != 0) {
            throw std::runtime_error(systemError("failed to install signal handler"));
        }
    }
}

bool isExecutableFile(const fs::path& path) {
    std::error_code error;
    return fs::is_regular_file(path, error) && !error && ::access(path.c_str(), X_OK) == 0;
}

std::string findExecutableOnPath(const std::string& name) {
    const char* rawPath = std::getenv("PATH");
    if (rawPath == nullptr) {
        return {};
    }

    const std::string pathEnvironment(rawPath);
    std::size_t start = 0;
    while (start <= pathEnvironment.size()) {
        const std::size_t end = pathEnvironment.find(':', start);
        const std::string directory = pathEnvironment.substr(
            start, end == std::string::npos ? std::string::npos : end - start);

        if (!directory.empty()) {
            const fs::path candidate = fs::path(directory) / name;
            if (isExecutableFile(candidate)) {
                std::error_code error;
                const fs::path absolute = fs::absolute(candidate, error);
                return (error ? candidate : absolute).string();
            }
        }

        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return {};
}

std::string findShell() {
    for (const char* candidate : {"/bin/bash", "/usr/bin/bash"}) {
        if (isExecutableFile(candidate)) {
            return candidate;
        }
    }

    const std::string bash = findExecutableOnPath("bash");
    if (!bash.empty()) {
        return bash;
    }

    for (const char* candidate : {"/bin/sh", "/usr/bin/sh"}) {
        if (isExecutableFile(candidate)) {
            return candidate;
        }
    }

    const std::string sh = findExecutableOnPath("sh");
    if (!sh.empty()) {
        return sh;
    }

    throw std::runtime_error("neither bash nor sh was found");
}

bool isBash(const std::string& shell) {
    std::string name = fs::path(shell).filename().string();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return name.rfind("bash", 0) == 0;
}

fs::path executablePath(const char* argv0) {
    std::error_code error;
#if defined(__linux__)
    const fs::path procPath = fs::read_symlink("/proc/self/exe", error);
    if (!error) {
        const fs::path canonicalPath = fs::canonical(procPath, error);
        return error ? procPath : canonicalPath;
    }
    error.clear();
#endif

    if (argv0 != nullptr && *argv0 != '\0') {
        const fs::path argument(argv0);
        if (argument.has_parent_path()) {
            const fs::path canonicalPath = fs::canonical(argument, error);
            if (!error) {
                return canonicalPath;
            }
            error.clear();
            const fs::path absolutePath = fs::absolute(argument, error);
            if (!error) {
                return absolutePath;
            }
            error.clear();
        } else {
            const std::string fromPath = findExecutableOnPath(argument.string());
            if (!fromPath.empty()) {
                return fromPath;
            }
        }
    }

    throw std::runtime_error("failed to resolve the native executable path");
}

void ensureResource(const fs::path& home, const char* name, std::string_view contents) {
    const fs::path target = home / name;
    std::error_code error;
    if (fs::is_regular_file(target, error) && !error) {
        return;
    }

    fs::path temporary = target;
    temporary += ".tmp." + std::to_string(static_cast<long long>(::getpid()));

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("failed to create " + temporary.string());
        }
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.flush();
        if (!output) {
            std::error_code ignored;
            fs::remove(temporary, ignored);
            throw std::runtime_error("failed to write " + temporary.string());
        }
        output.close();
        if (!output) {
            std::error_code ignored;
            fs::remove(temporary, ignored);
            throw std::runtime_error("failed to close " + temporary.string());
        }
    }

    fs::rename(temporary, target, error);
    if (error) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw std::runtime_error(
            "failed to install " + target.string() + ": " + error.message());
    }

    if (::chmod(target.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) != 0) {
        throw std::runtime_error(systemError("failed to set permissions on " + target.string()));
    }
}

void setEnvironment(const char* name, const std::string& value) {
    if (::setenv(name, value.c_str(), 1) != 0) {
        throw std::runtime_error(systemError(std::string("failed to set ") + name));
    }
}

BackgroundProcess startBackground(const fs::path& script, const std::string& shell) {
    const int nullInput = ::open("/dev/null", O_RDONLY);
    if (nullInput < 0) {
        throw std::runtime_error(systemError("failed to open /dev/null"));
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        const int savedErrno = errno;
        (void)::close(nullInput);
        errno = savedErrno;
        throw std::runtime_error(systemError("failed to fork background startup script"));
    }

    if (pid == 0) {
        if (::setpgid(0, 0) != 0) {
            (void)::dprintf(
                STDERR_FILENO,
                "[shim-native] failed to create background process group: %s\n",
                std::strerror(errno));
            _exit(126);
        }
        if (::dup2(nullInput, STDIN_FILENO) < 0) {
            (void)::dprintf(
                STDERR_FILENO,
                "[shim-native] failed to redirect background stdin: %s\n",
                std::strerror(errno));
            _exit(126);
        }
        if (nullInput > STDERR_FILENO) {
            (void)::close(nullInput);
        }

        char* const arguments[] = {
            const_cast<char*>(shell.c_str()),
            const_cast<char*>(script.c_str()),
            nullptr,
        };
        ::execv(shell.c_str(), arguments);
        (void)::dprintf(
            STDERR_FILENO,
            "[shim-native] failed to execute background shell: %s\n",
            std::strerror(errno));
        _exit(127);
    }

    (void)::close(nullInput);

    bool hasProcessGroup = false;
    if (::setpgid(pid, pid) == 0) {
        hasProcessGroup = true;
    } else if (errno == EACCES && ::getpgid(pid) == pid) {
        hasProcessGroup = true;
    }

    return BackgroundProcess{pid, hasProcessGroup};
}

int waitForForegroundProcess(pid_t pid) {
    int status = 0;
    bool terminationEscalationStarted = false;
    auto killDeadline = std::chrono::steady_clock::time_point {};

    for (;;) {
        const pid_t result = ::waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            break;
        }
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error(systemError("failed to wait for interactive shell"));
        }

        const auto now = std::chrono::steady_clock::now();
        if (gTerminationRequested != 0 && !terminationEscalationStarted) {
            (void)::kill(pid, SIGTERM);
            killDeadline = now + kForegroundStopTimeout;
            terminationEscalationStarted = true;
        } else if (terminationEscalationStarted && now >= killDeadline) {
            (void)::kill(pid, SIGKILL);
            killDeadline = std::chrono::steady_clock::time_point::max();
        }

        std::this_thread::sleep_for(kProcessPollInterval);
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

int runInteractiveShell(const fs::path& pathFile, const std::string& shell) {
    const pid_t pid = ::fork();
    if (pid < 0) {
        throw std::runtime_error(systemError("failed to fork interactive shell"));
    }

    if (pid == 0) {
        std::vector<std::string> arguments;
        if (isBash(shell)) {
            arguments = {shell, "--rcfile", pathFile.string(), "-i"};
        } else {
            arguments = {shell, "-i"};
        }

        std::vector<char*> argumentPointers;
        argumentPointers.reserve(arguments.size() + 1);
        for (std::string& argument : arguments) {
            argumentPointers.push_back(argument.data());
        }
        argumentPointers.push_back(nullptr);

        ::execv(shell.c_str(), argumentPointers.data());
        (void)::dprintf(
            STDERR_FILENO,
            "[shim-native] failed to execute shell: %s\n",
            std::strerror(errno));
        _exit(127);
    }

    gForegroundPid = pid;
    const int exitCode = waitForForegroundProcess(pid);
    gForegroundPid = -1;
    return exitCode;
}

bool processGroupIsAlive(pid_t processGroup) {
    if (::kill(-processGroup, 0) == 0) {
        return true;
    }
    return errno != ESRCH;
}

void stopBackground(const BackgroundProcess& background) noexcept {
    if (background.pid <= 0) {
        return;
    }

    if (background.hasProcessGroup) {
        (void)::kill(-background.pid, SIGTERM);
    }
    (void)::kill(background.pid, SIGTERM);

    bool childReaped = false;
    const auto deadline = std::chrono::steady_clock::now() + kBackgroundStopTimeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (!childReaped) {
            int status = 0;
            const pid_t result = ::waitpid(background.pid, &status, WNOHANG);
            if (result == background.pid || (result < 0 && errno == ECHILD)) {
                childReaped = true;
            }
        }

        const bool groupAlive = background.hasProcessGroup && processGroupIsAlive(background.pid);
        if (childReaped && !groupAlive) {
            return;
        }
        std::this_thread::sleep_for(kProcessPollInterval);
    }

    if (background.hasProcessGroup) {
        (void)::kill(-background.pid, SIGKILL);
    }
    (void)::kill(background.pid, SIGKILL);

    if (!childReaped) {
        while (::waitpid(background.pid, nullptr, 0) < 0 && errno == EINTR) {
        }
    }
}

void announceShell(const std::string& shell, const fs::path& pathFile) {
    std::cout << "[shim-native] using shell: " << shell << '\n';
    std::cout << "[shim-native] path file: " << pathFile.string() << '\n';
    if (!isBash(shell)) {
        std::cout << "[shim-native] warning: bash was not found, falling back to sh\n";
        std::cout << "[shim-native] warning: bash-only syntax in server__autoLaunch.sh may not work\n";
    }
    std::cout.flush();
}

}  // namespace

int main(int argc, char* argv[]) {
    (void)argc;

    BackgroundProcess background;
    int exitCode = 0;

    try {
        installSignalHandlers();

        const fs::path executable = executablePath(argc > 0 ? argv[0] : nullptr);
        const fs::path home = executable.parent_path();
        if (home.empty()) {
            throw std::runtime_error("failed to determine the executable directory");
        }

        ensureResource(home, kAutoLaunchFileName, shim::resources::kAutoLaunchScript);
        ensureResource(home, kPathFileName, shim::resources::kPathScript);

        if (::chdir(home.c_str()) != 0) {
            throw std::runtime_error(systemError("failed to change directory to " + home.string()));
        }

        const fs::path script = home / kAutoLaunchFileName;
        const fs::path pathFile = home / kPathFileName;
        const std::string shell = findShell();

        setEnvironment("SHIM_HOME", home.string());
        if (!isBash(shell)) {
            setEnvironment("ENV", pathFile.string());
        }

        announceShell(shell, pathFile);

        background = startBackground(script, shell);
        gBackgroundPid = background.pid;
        gBackgroundHasProcessGroup = background.hasProcessGroup ? 1 : 0;

        exitCode = runInteractiveShell(pathFile, shell);
    } catch (const std::exception& error) {
        std::cerr << "[shim-native] failed to start console: " << error.what() << '\n';
        exitCode = 1;
    }

    gForegroundPid = -1;
    stopBackground(background);
    gBackgroundPid = -1;
    gBackgroundHasProcessGroup = 0;

    return exitCode;
}
