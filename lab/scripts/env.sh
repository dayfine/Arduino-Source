#!/bin/sh
# Shared settings for the lab console scripts. Every value can be overridden from the environment.
#   LAB_CONSOLE        console letter (default B)
#   LAB_PROFILE_NAME   SerialPrograms profile of the lab app (default lab-<console>)
#   LAB_PROFILE        that profile's folder
#   LAB_CONTROL        folder with labctl.sh / lablock.sh / cfrun.sh. Default: lab/control next to
#                      these scripts, else $PA_AGENTS_DIR/lab/control (PA_AGENTS_DIR defaults to
#                      ~/Projects/Arduino-Source-agents), else the old lab-tree copy
#                      ~/Projects/Arduino-Source-frlglab/devtools/lab.
#   LAB_DIR            lablock.sh's lock root (default ~/.lab). Older versions of this file used
#                      it for the labctl folder; a LAB_DIR holding labctl.sh is still read that way.
#   LABLOCK_TOKEN_FILE file holding the console lock token from lablock.sh (optional;
#                      LABLOCK_TOKEN set directly also works)
LAB_CONSOLE=${LAB_CONSOLE:-B}
LAB_PROFILE_NAME=${LAB_PROFILE_NAME:-lab-$LAB_CONSOLE}
LAB_PROFILE=${LAB_PROFILE:-"$HOME/Library/Application Support/SerialPrograms/Profiles/$LAB_PROFILE_NAME"}
LAB_SCRIPTS=${LAB_SCRIPTS:-$(cd "$(dirname "$0")" && pwd)}
if [ -z "${LAB_CONTROL:-}" ] && [ -n "${LAB_DIR:-}" ] && [ -f "$LAB_DIR/labctl.sh" ]; then
  LAB_CONTROL=$LAB_DIR
  unset LAB_DIR
fi
if [ -z "${LAB_CONTROL:-}" ]; then
  for d in "$LAB_SCRIPTS/../control" \
           "${PA_AGENTS_DIR:-$HOME/Projects/Arduino-Source-agents}/lab/control" \
           "$HOME/Projects/Arduino-Source-frlglab/devtools/lab"; do
    if [ -f "$d/labctl.sh" ]; then LAB_CONTROL=$(cd "$d" && pwd); break; fi
  done
fi
if [ -z "${LABLOCK_TOKEN:-}" ] && [ -n "${LABLOCK_TOKEN_FILE:-}" ] && [ -f "$LABLOCK_TOKEN_FILE" ]; then
  LABLOCK_TOKEN=$(cat "$LABLOCK_TOKEN_FILE")
fi
export LAB_CONSOLE LAB_PROFILE_NAME LAB_PROFILE LAB_CONTROL LABLOCK_TOKEN LAB_SCRIPTS
