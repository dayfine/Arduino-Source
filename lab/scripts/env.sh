#!/bin/sh
# Shared settings for the lab console scripts. Every value can be overridden from the environment.
#   LAB_CONSOLE        console letter (default B)
#   LAB_PROFILE_NAME   SerialPrograms profile of the lab app (default lab-<console>)
#   LAB_PROFILE        that profile's folder
#   LAB_DIR            devtools/lab of a lab tree that has labctl.sh
#   LABLOCK_TOKEN_FILE file holding the console lock token from lablock.sh (optional;
#                      LABLOCK_TOKEN set directly also works)
LAB_CONSOLE=${LAB_CONSOLE:-B}
LAB_PROFILE_NAME=${LAB_PROFILE_NAME:-lab-$LAB_CONSOLE}
LAB_PROFILE=${LAB_PROFILE:-"$HOME/Library/Application Support/SerialPrograms/Profiles/$LAB_PROFILE_NAME"}
LAB_DIR=${LAB_DIR:-"$HOME/Projects/Arduino-Source-frlglab/devtools/lab"}
if [ -z "${LABLOCK_TOKEN:-}" ] && [ -n "${LABLOCK_TOKEN_FILE:-}" ] && [ -f "$LABLOCK_TOKEN_FILE" ]; then
  LABLOCK_TOKEN=$(cat "$LABLOCK_TOKEN_FILE")
fi
LAB_SCRIPTS=${LAB_SCRIPTS:-$(cd "$(dirname "$0")" && pwd)}
export LAB_CONSOLE LAB_PROFILE_NAME LAB_PROFILE LAB_DIR LABLOCK_TOKEN LAB_SCRIPTS
