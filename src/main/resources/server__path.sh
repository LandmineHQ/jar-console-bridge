#!/usr/bin/env sh

# -----------------------------------------------------------------------------
# PATH configuration
# -----------------------------------------------------------------------------
# This file is sourced by both server__autoLaunch.sh and the interactive console.
# Keep PATH-related setup here. Application variables and startup commands
# belong in server__autoLaunch.sh.

# SHIM_HOME is set by server.jar. The fallback keeps this file usable when it is
# sourced manually from the project root.
if [ -z "${SHIM_HOME:-}" ]; then
    SHIM_HOME=$(pwd)
fi

# Add a directory to PATH only when it is not already present.
_add_path() {
    case ":${PATH:-}:" in
        *":$1:"*) ;;
        *) PATH="$1${PATH:+:$PATH}" ;;
    esac
}

# -----------------------------------------------------------------------------
# Bundled tools
# -----------------------------------------------------------------------------
_add_path "$SHIM_HOME/opt/node-v20.11.0-linux-x64/bin"
_add_path "$SHIM_HOME/.local/bin"

# -----------------------------------------------------------------------------
# Custom PATH entries
# -----------------------------------------------------------------------------
# Add executable directories below:
# _add_path "$SHIM_HOME/opt/jdk-21/bin"
# _add_path "$SHIM_HOME/tools/bin"

unset -f _add_path
