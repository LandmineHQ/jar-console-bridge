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
| `.github/workflows/build.yml` | Builds the jar on CI events and uploads the `server-jar` artifact. |
| `.github/workflows/release.yml` | Manually attaches a selected build artifact to a draft or existing release. |
| `.gitattributes` | Enforces line endings for shell, Windows, and text files. |
| `.gitignore` | Excludes Maven output, jars, runtime-extracted scripts, IDE files, logs, and local secrets. |
| `.mvn/wrapper/maven-wrapper.properties` | Pins the Maven distribution used by the wrapper. |
| `AGENTS.md` | Repository-specific maintenance rules for coding agents and contributors. |
| `README.md` | Short project description and build instructions. |
| `REPO_MAP.md` | Detailed repository structure and file responsibilities. |
| `mvnw` | Maven Wrapper entry point for Linux and macOS. |
| `mvnw.cmd` | Maven Wrapper entry point for Windows. |
| `pom.xml` | Maven coordinates, Java release target, jar name, and manifest main class. |
| `src/main/java/ShimMain.java` | Locates `server.jar`, selects Bash or sh, starts the background script, and attaches the interactive console. |
| `src/main/resources/server__autoLaunch.sh` | Resolves its own directory, loads the shared PATH file, and contains background startup commands. |
| `src/main/resources/server__path.sh` | Shared PATH-only configuration for the background script and interactive console. |

## Runtime Flow

1. `ShimMain` resolves the directory containing `server.jar`.
2. Missing shell resources are extracted next to the jar.
3. `server__autoLaunch.sh` starts in the background with empty stdin.
4. An interactive shell starts in the foreground with inherited panel stdio.
5. Bash is used when available; otherwise the bridge falls back to sh.
6. The background process is terminated when the foreground shell exits.

## GitHub Workflows

### Build

`build.yml` runs on branch pushes, version tag pushes, pull requests targeting
`main`, and manual dispatch. It builds `target/server.jar`, validates the
embedded resources, and uploads the artifact as `server-jar`.

### Release

`release.yml` runs through manual dispatch with two inputs:

- `build_id`: a successful GitHub Actions run ID
- `version`: a semantic version such as `v1.0.0`

The workflow validates the selected build, downloads `server-jar`, and renames
the asset with the version, build ID, and commit SHA. If the release exists,
the asset is added to it. Otherwise, a draft release is created.

## Generated Files

These files are generated and must not be committed:

| Path | Source |
| --- | --- |
| `target/` | Maven compilation and packaging output. |
| `target/server.jar` | Final build artifact. |
| `/server.jar` | Optional manually copied runtime artifact. |
| `/server__autoLaunch.sh` | Runtime extraction when the jar is placed in the repository root. |
| `/server__path.sh` | Runtime extraction when the jar is placed in the repository root. |

The repository-root script patterns are anchored with `/`, so the source files
under `src/main/resources/` remain tracked.
