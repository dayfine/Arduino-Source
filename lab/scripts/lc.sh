#!/bin/sh
# lc.sh ARGS... : run labctl.sh against the lab app (console and profile from env.sh).
# Examples: lc.sh state | lc.sh select NintendoSwitch:CommandFileRunner | lc.sh start | lc.sh stop
. "$(dirname "$0")/env.sh"
export LABCTL_WAIT=${LABCTL_WAIT:-180} LAB_OWNER=${LAB_OWNER:-$LAB_PROFILE_NAME}
[ -f "${LAB_CONTROL:-}/labctl.sh" ] || { echo "lc.sh: no labctl.sh found (set LAB_CONTROL; see env.sh)" >&2; exit 2; }
exec sh "$LAB_CONTROL/labctl.sh" "$@"
