import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.regex.Pattern;

public final class ShimMain {
    private static final String SCRIPT_NAME = "server__autoLaunch.sh";
    private static final String PATH_FILE_NAME = "server__path.sh";

    private ShimMain() {
    }

    public static void main(String[] args) {
        int exitCode = 0;
        try {
            Path home = locateHome();
            Path script = ensureResource(home, SCRIPT_NAME);
            Path pathFile = ensureResource(home, PATH_FILE_NAME);
            String shell = shellCommand();
            announceShell(shell, pathFile);
            Process background = startBackground(home, script, shell);
            try {
                exitCode = runConsole(home, shell, pathFile);
            } finally {
                background.destroyForcibly();
            }
        } catch (Exception e) {
            System.err.println("[shim] failed to start console: " + e.getMessage());
            exitCode = 1;
        }
        System.exit(exitCode);
    }

    private static Path locateHome() throws Exception {
        try {
            Path source = Paths.get(ShimMain.class.getProtectionDomain().getCodeSource().getLocation().toURI());
            Path dir = Files.isDirectory(source) ? source : source.getParent();
            if (dir != null) {
                return dir.toAbsolutePath();
            }
        } catch (Exception ignored) {
            // Fall back to the process working directory below.
        }
        return Paths.get("").toAbsolutePath();
    }

    private static Path ensureResource(Path home, String name) throws IOException {
        Path target = home.resolve(name);
        if (Files.isRegularFile(target)) {
            return target;
        }

        try (InputStream in = ShimMain.class.getResourceAsStream("/" + name)) {
            if (in == null) {
                throw new IOException("embedded " + name + " is missing");
            }
            Files.copy(in, target);
        }
        return target;
    }

    private static Process startBackground(Path home, Path script, String shell) throws IOException {
        File emptyInput = File.createTempFile("autoLaunch-shim", ".stdin");
        emptyInput.deleteOnExit();

        ProcessBuilder pb = processBuilder(home, shell, script.toString());
        pb.redirectInput(emptyInput);
        pb.redirectOutput(ProcessBuilder.Redirect.INHERIT);
        pb.redirectError(ProcessBuilder.Redirect.INHERIT);
        return pb.start();
    }

    private static int runConsole(Path home, String shell, Path pathFile) throws IOException, InterruptedException {
        ProcessBuilder pb;
        if (isBash(shell)) {
            pb = processBuilder(home, shell, "--rcfile", pathFile.toString(), "-i");
        } else {
            pb = processBuilder(home, shell, "-i");
            pb.environment().put("ENV", pathFile.toString());
        }
        pb.inheritIO();
        return pb.start().waitFor();
    }

    private static ProcessBuilder processBuilder(Path home, String... command) {
        ProcessBuilder pb = new ProcessBuilder(command);
        pb.directory(home.toFile());
        pb.environment().put("SHIM_HOME", home.toString());
        return pb;
    }

    private static void announceShell(String shell, Path pathFile) {
        System.out.println("[shim] using shell: " + shell);
        System.out.println("[shim] path file: " + pathFile);
        if (!isBash(shell)) {
            System.out.println("[shim] warning: bash was not found, falling back to sh");
            System.out.println("[shim] warning: bash-only syntax in server__autoLaunch.sh may not work");
        }
    }

    private static boolean isBash(String shell) {
        String name = new File(shell).getName().toLowerCase();
        return name.startsWith("bash");
    }

    private static String shellCommand() {
        String bash = findShell("bash", new String[] { "/bin/bash", "/usr/bin/bash" });
        if (bash != null) {
            return bash;
        }

        String sh = findShell("sh", new String[] { "/bin/sh", "/usr/bin/sh" });
        if (sh != null) {
            return sh;
        }

        return File.separatorChar == '/' ? "sh" : "sh.exe";
    }

    private static String findShell(String name, String[] fixedCandidates) {
        if (File.separatorChar == '/') {
            for (String candidate : fixedCandidates) {
                if (new File(candidate).canExecute()) {
                    return candidate;
                }
            }
        }

        String fileName = File.separatorChar == '/' ? name : name + ".exe";
        String pathEnv = System.getenv("PATH");
        if (pathEnv != null) {
            String separator = Pattern.quote(File.pathSeparator);
            for (String dir : pathEnv.split(separator)) {
                if (dir.isEmpty()) {
                    continue;
                }
                File candidate = new File(dir, fileName);
                if (candidate.canExecute()) {
                    return candidate.getAbsolutePath();
                }
            }
        }
        return null;
    }
}
