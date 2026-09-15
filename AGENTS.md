# AGENTS.md

## Project Purpose

This repository builds a console bridge jar for game panels that can only run
`java -jar server.jar`. The jar connects the panel's stdin, stdout, and stderr
to an interactive shell and starts the embedded background startup script.

## Build And Verification

Build on Windows:

```powershell
.\mvnw.cmd clean package
```

Build on Linux or macOS:

```bash
./mvnw clean package
```

Expected artifact:

```text
target/server.jar
```

Run these checks after changing Java or shell resources:

```powershell
bash -n src/main/resources/server__autoLaunch.sh
sh -n src/main/resources/server__autoLaunch.sh
sh -n src/main/resources/server__path.sh
jar tf target/server.jar
```

Before finishing, verify that the jar contains:

- `META-INF/MANIFEST.MF`
- `ShimMain.class`
- `server__autoLaunch.sh`
- `server__path.sh`

## Critical Invariants

- Keep the Maven output name as `server.jar` for panel compatibility.
- Keep the manifest main class as `ShimMain`.
- Keep `server__autoLaunch.sh` running in the background.
- Keep the interactive shell in the foreground with inherited stdio.
- Prefer Bash and fall back to `sh` only when Bash is unavailable.
- Keep `server__path.sh` limited to PATH handling. Do not place application
  variables or service commands in it.
- Both `server__autoLaunch.sh` and the interactive shell must load
  `server__path.sh`.
- Resolve the jar or script directory before running scripts. Do not depend on
  an external current working directory.
- Preserve Java 8 bytecode compatibility unless the project requirements
  explicitly change.

## Source Layout

- `src/main/java/ShimMain.java`: Java process launcher and shell selection.
- `src/main/resources/server__autoLaunch.sh`: background startup commands.
- `src/main/resources/server__path.sh`: shared PATH configuration.
- `pom.xml`: Maven build configuration.
- `mvnw` and `mvnw.cmd`: Maven Wrapper entry points.

See `REPO_MAP.md` for the complete tracked and generated file map.

## Development Notes

- Use UTF-8 for text files.
- Keep shell scripts POSIX-compatible where practical so the `sh` fallback
  remains usable.
- Keep `server__path.sh` idempotent. Repeated sourcing must not create duplicate
  PATH entries.
- Do not commit `target/` or runtime-extracted scripts from the repository root.
- Use Conventional Commits for commit messages, for example
  `fix: handle missing bash fallback`.
