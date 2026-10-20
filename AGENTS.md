# AGENTS.md

## Project Purpose

This repository builds two equivalent console bridges for game panels:

- A Java jar for panels restricted to `java -jar server.jar`.
- A native POSIX/Linux binary for panels that can launch an executable directly.

Both launchers connect the panel's stdin, stdout, and stderr to an interactive
shell and start the embedded background startup script.

## Build And Verification

Build the Java jar on Windows:

```powershell
.\mvnw.cmd clean package
```

Build the Java jar on Linux or macOS:

```bash
./mvnw clean package
```

Build the native binary on Linux:

```bash
cmake -S native -B build/native \
  -DCMAKE_BUILD_TYPE=Release \
  -DSHIM_FULLY_STATIC=ON \
  -DSHIM_WARNINGS_AS_ERRORS=ON
cmake --build build/native --parallel
```

Expected artifacts:

```text
target/server.jar
build/native/server
```

Run these checks after changing Java or shell resources:

```powershell
bash -n src/main/resources/server__autoLaunch.sh
sh -n src/main/resources/server__autoLaunch.sh
sh -n src/main/resources/server__path.sh
jar tf target/server.jar
actionlint .github/workflows/*.yml
```

Before finishing, verify that the jar contains:

- `META-INF/MANIFEST.MF`
- `ShimMain.class`
- `server__autoLaunch.sh`
- `server__path.sh`

For native changes, also verify:

- The native target compiles with `-Wall -Wextra -Wpedantic -Werror`.
- `build/native/server` is executable.
- The embedded scripts extract identically to `src/main/resources/`.
- The binary starts the background script and an interactive shell in a PTY.

## Critical Invariants

- Keep the Maven output name as `server.jar` for panel compatibility.
- Keep the build workflow artifact names as `server_jar` and
  `server_linux_x64`. Do not add hashes to build artifact names.
- Release assets must be named `server_jar__<sha256>.jar` and
  `server_linux_x64__<sha256>`, where `<sha256>` is the full lowercase hash of
  the uploaded file.
- Publish a `SHA256SUMS` file alongside every release.
- Keep the manifest main class as `ShimMain`.
- Keep `server__autoLaunch.sh` running in the background.
- Keep the interactive shell in the foreground with inherited stdio.
- Prefer Bash and fall back to `sh` only when Bash is unavailable.
- Keep `server__path.sh` limited to PATH handling. Do not place application
  variables or service commands in it.
- Both `server__autoLaunch.sh` and the interactive shell must load
  `server__path.sh`.
- Resolve the jar, binary, or script directory before running scripts. Do not
  depend on an external current working directory.
- Preserve Java 8 bytecode compatibility unless the project requirements
  explicitly change.
- Both launchers must embed the same shell files from `src/main/resources/`.
  Do not copy those files into a second source location.
- The native launcher currently targets POSIX/Linux x86_64. Do not claim Windows
  or ARM support without adding and testing the corresponding implementation.
- The native executable must not require Java or an external C++ runtime.
- The CI and release native artifact should be fully static. Keep
  `SHIM_FULLY_STATIC=ON` in the release build unless the target requirements
  explicitly change.

## Source Layout

- `src/main/java/ShimMain.java`: Java process launcher and shell selection.
- `src/main/resources/server__autoLaunch.sh`: shared background startup script.
- `src/main/resources/server__path.sh`: shared PATH-only configuration.
- `native/src/main.cpp`: C++17 process launcher and shell selection.
- `native/src/embedded_resources.hpp.in`: generated-header template used to embed
  the shared shell resources.
- `native/CMakeLists.txt`: native build configuration.
- `pom.xml`: Maven build configuration.
- `mvnw` and `mvnw.cmd`: Maven Wrapper entry points.

See `REPO_MAP.md` for the complete tracked and generated file map.

## Development Notes

- Use UTF-8 for text files.
- Keep shell scripts POSIX-compatible where practical so the `sh` fallback
  remains usable.
- Keep `server__path.sh` idempotent. Repeated sourcing must not create duplicate
  PATH entries.
- Do not commit `target/` or `build/`.
- Do not commit runtime-extracted scripts from the repository root.
- Use Conventional Commits for commit messages, for example
  `feat: add native console bridge`.

## CI/CD Behavior

- `.github/workflows/build.yml` builds on pushes, pull requests, and manual
  dispatches. It uploads `target/server.jar` as `server_jar` and
  `build/native/server` as `server_linux_x64`.
- The native CI job runs on Ubuntu 22.04 and includes a PTY smoke test.
- `.github/workflows/release.yml` is manually dispatched with a successful
  GitHub Actions run ID and a semantic version.
- The release workflow verifies the selected run, downloads both artifacts,
  computes SHA-256 hashes, and publishes `server_jar__<sha256>.jar`,
  `server_linux_x64__<sha256>`, and `SHA256SUMS`.
- If the version does not exist, the workflow creates a draft release.
- If the release already exists, the workflow adds the new build assets without
  replacing other build assets.
