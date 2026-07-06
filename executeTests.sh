#!/usr/bin/env bash
#
# executeTests.sh — run the host-side Unity/CMock unit test suite.
#
# This only builds and runs tests on your PC (gcc), separate from the
# STM32CubeIDE arm-none-eabi target build. See project.yml and test/.
#
# Usage:
#   ./executeTests.sh              # run every test
#   ./executeTests.sh test_AMS_sensors   # run a single test file (no "test:" prefix)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Ceedling installs its executable under the user gem bin dir, which may not
# be on PATH in a fresh shell — add it here so this script works regardless.
GEM_USER_BIN="$(ruby -e 'puts Gem.user_dir' 2>/dev/null)/bin"
if [ -d "$GEM_USER_BIN" ]; then
    export PATH="$GEM_USER_BIN:$PATH"
fi

if ! command -v ceedling >/dev/null 2>&1; then
    echo "ERROR: 'ceedling' not found." >&2
    echo "Install with: sudo apt install -y ruby-full build-essential && gem install ceedling --user-install" >&2
    exit 1
fi

if [ "$#" -eq 0 ]; then
    exec ceedling test:all
else
    exec ceedling "test:$1"
fi
