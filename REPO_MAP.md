# Repository Map

## Tracked Files

```text
.
├── .github/
│   └── workflows/
│       ├── build.yml
│       └── release.yml
├── .gitattributes
├── .gitignore
├── .mvn/
│   └── wrapper/
│       └── maven-wrapper.properties
├── AGENTS.md
├── README.md
├── REPO_MAP.md
├── mvnw
├── mvnw.cmd
├── pom.xml
├── native/
│   ├── CMakeLists.txt
│   └── src/
│       ├── embedded_resources.hpp.in
│       └── main.cpp
└── src/
    └── main/
        ├── java/
        │   └── ShimMain.java
        └── resources/
            ├── server__autoLaunch.sh
            └── server__path.sh
```

## File Responsibilities

| Path | Responsibility |
| --- | --- |
| `.github/workflows/build.yml` | Builds and verifies both delivery forms, then uploads `server-jar` and `server-binary`. |
| `.github/workflows/release.yml` | Manually attaches both artifacts from a selected build to a draft or existing release. |
| `.gitattributes` | Enforces line endings for shell, Windows, and text files. |
| `.gitignore` | Excludes Maven/CMake output, jars, runtime-extracted scripts, IDE files, logs, and local secrets. |
| `.mvn/wrapper/maven-wrapper.properties` | Pins the Maven distribution used by the wrapper. |
| `AGENTS.md` | Repository-specific maintenance rules for coding agents and contributors. |
| `README.md` | User-facing description, build instructions, and distribution formats. |
| `REPO_MAP.md` | Detailed repository structure and file responsibilities. |
| `mvnw` | Maven Wrapper entry point for Linux and macOS. |
| `mvnw.cmd` | Maven Wrapper entry point for Windows. |
| `pom.xml` | Maven coordinates, Java release target, jar name, and manifest main class. |
| `native/CMakeLists.txt` | Native C++ build, warnings, runtime linking, and shared-resource embedding. |
| `native/src/main.cpp` | POSIX native launcher: resolves its directory, extracts scripts, selects a shell, starts the background script, and attaches the interactive console. |
| `native/src/embedded_resources.hpp.in` | CMake template that turns the shared shell files into C++ string constants. |
| `src/main/java/ShimMain.java` | Java launcher: locates the jar, extracts scripts, selects Bash or sh, starts the background script, and attaches the interactive console. |
| `src/main/resources/server__autoLaunch.sh` | Shared background startup commands. |
| `src/main/resources/server__path.sh` | Shared PATH-only configuration for both launchers. |

## Runtime Flow

### Java jar

1. `ShimMain` resolves the directory containing `server.jar`.
2. Missing shell resources are extracted next to the jar.
3. `server__autoLaunch.sh` starts in the background with empty stdin.
4. An interactive shell starts in the foreground with inherited panel stdio.
5. Bash is used when available; otherwise the launcher falls back to sh.
6. The background process is terminated when the foreground shell exits.

### Native binary

1. The C++ launcher resolves its own path through `/proc/self/exe` and uses its
   parent directory as the runtime home.
2. Missing shell resources are atomically extracted next to the executable.
3. The process changes its working directory to the executable directory.
4. `server__autoLaunch.sh` starts in its own process group with empty stdin.
5. An interactive shell starts in the foreground with inherited panel stdio.
6. Bash is used when available; otherwise the launcher falls back to sh.
7. The foreground shell exit is propagated as the binary exit code.
8. The background process group receives SIGTERM, then SIGKILL after a timeout.

## GitHub Workflows

### Build

`build.yml` runs on branch pushes, version tag pushes, pull requests targeting
`main`, and manual dispatch.

- `build-jar` runs on `ubuntu-latest`, validates shell syntax, builds with
  Maven, verifies the embedded resources, and uploads `server-jar`.
- `build-native` runs on `ubuntu-22.04`, compiles with warnings as errors,
  verifies the embedded scripts, performs a PTY smoke test, and uploads
  `server-binary`.

### Release

`release.yml` runs through manual dispatch with two inputs:

- `build_id`: a successful GitHub Actions run ID
- `version`: a semantic version such as `v1.0.0`

The workflow validates the selected build, downloads both `server-jar` and
`server-binary`, and gives each artifact a version/build/commit-specific name.
If the release exists, both assets are added to it. Otherwise, a draft release
is created with both assets.

## Generated Files

These files are generated and must not be committed:

| Path | Source |
| --- | --- |
| `target/` | Maven compilation and packaging output. |
| `target/server.jar` | Java build artifact. |
| `build/` | CMake build output. |
| `build/native/server` | Native build artifact. |
| `build/native/generated/embedded_resources.hpp` | Generated C++ resource header. |
| `/server.jar` | Optional manually copied Java runtime artifact. |
| `/server` | Optional manually copied native runtime artifact. |
| `/server__autoLaunch.sh` | Runtime extraction when a launcher is placed in the repository root. |
| `/server__path.sh` | Runtime extraction when a launcher is placed in the repository root. |

The repository-root script patterns are anchored with `/`, so the source files
under `src/main/resources/` remain tracked.
