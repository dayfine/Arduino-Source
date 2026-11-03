#!/bin/sh
# lc.sh ARGS... : run labctl.sh against the lab app (console and profile from env.sh).
# Examples: lc.sh state | lc.sh select NintendoSwitch:CommandFileRunner | lc.sh start | lc.sh stop
. "$(dirname "$0")/env.sh"
export LABCTL_WAIT=${LABCTL_WAIT:-180} LAB_OWNER=${LAB_OWNER:-$LAB_PROFILE_NAME}
[ -f "$LAB_DIR/labctl.sh" ] || { echo "lc.sh: no labctl.sh in LAB_DIR=$LAB_DIR" >&2; exit 2; }
cd "$LAB_DIR" && exec sh labctl.sh "$@"
