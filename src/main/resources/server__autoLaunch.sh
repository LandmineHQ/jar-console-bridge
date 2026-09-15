#!/usr/bin/env bash

# -----------------------------------------------------------------------------
# Strict mode
# -----------------------------------------------------------------------------
# -e: exit when a command returns a non-zero status
# -u: fail when an undefined variable is used
# pipefail: preserve failures from commands connected by a pipe when supported
set -eu
if (set -o pipefail) 2>/dev/null; then
    set -o pipefail
fi

# -----------------------------------------------------------------------------
# Script location
# -----------------------------------------------------------------------------
# Resolve the directory containing this script without relying on the caller's
# current working directory, then run from that directory.
case "$0" in
    */*) SCRIPT_DIR=${0%/*} ;;
    *) SCRIPT_DIR=. ;;
esac
SCRIPT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR" && pwd)

cd "$SCRIPT_DIR" || exit 1

SHIM_HOME="$SCRIPT_DIR"
export SHIM_HOME

# -----------------------------------------------------------------------------
# PATH configuration
# -----------------------------------------------------------------------------
PATH_FILE="$SHIM_HOME/server__path.sh"

if [ ! -f "$PATH_FILE" ]; then
    echo "[autoLaunch] path file not found: $PATH_FILE" >&2
    exit 1
fi

# Load the same PATH configuration used by the interactive console.
. "$PATH_FILE"

# -----------------------------------------------------------------------------
# Runtime information
# -----------------------------------------------------------------------------
echo "[autoLaunch] background script started"
echo "[autoLaunch] working dir: $PWD"
echo "[autoLaunch] node path: $(command -v node || echo 'node not found')"
echo "[autoLaunch] javm path: $(command -v javm || echo 'javm not found')"

# -----------------------------------------------------------------------------
# Startup commands
# -----------------------------------------------------------------------------
# Add startup commands below. Use one block per service and keep directory
# changes local when later commands should run from the original directory.
#
# Start MCSManager:
# (
#     cd "$SHIM_HOME/mcsmanager"
#     exec bash start-daemon.sh
# )

echo "[autoLaunch] no startup command configured"
