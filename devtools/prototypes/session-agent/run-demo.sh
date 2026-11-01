#!/bin/sh
# Standalone native C++ demo; does not build or launch SerialPrograms.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/pa-session-agent.XXXXXX")
"${CXX:-c++}" -std=c++23 -Wall -Wextra -Werror -I "$repo" \
    "$repo/SerialPrograms/Source/Integrations/SessionAgentPrototype/PrototypeDemo.cpp" \
    -o "$build_dir/demo"
exec "$build_dir/demo" "$@"
