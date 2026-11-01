#!/bin/sh
# cfrun.sh: send one batch of console commands (presses, screenshots,
# detector checks; see SerialPrograms/Source/CommonFramework/Windows/Lab.md)
# to the app's lab channel and print its journal.
#
#   cfrun.sh [-p PROFILE] NAME < batch.txt
#       Takes the console's lock (lablock.sh; fails at once if another train
#       holds it), sends the batch as request NAME, waits for outbox/NAME.out
#       and prints it. Exit 0 when every line passed, 1 when a line failed or
#       the app never took the file, 3 when the console is locked.
#
# PROFILE is the app's user-files folder (-p, else $LAB_PROFILE; see
# labctl.sh). Screenshots land in PROFILE/Lab/outbox as NAME_label.png.
# $CFRUN_WAIT (default 600) is the longest a batch may take, in seconds.
# $LAB_OWNER, when set, goes on the request's owner line (labctl.sh).
#
# No program needs to run: the channel presses for the selected program
# while it is stopped. While a program is RUNNING the app refuses the
# presses itself ("error N (press ...): Program is RUNNING; ...") and runs
# only the lines that read the screen; the journal says so. Never stop a
# program to make room for a batch.

here=$(dirname "$0")
profile=$LAB_PROFILE
if [ "$1" = -p ]; then profile=$2; shift 2; fi
[ -n "$profile" ] || { echo "cfrun: set LAB_PROFILE or pass -p PROFILE" >&2; exit 2; }
[ $# = 1 ] && [ -n "$1" ] || { echo "usage: cfrun.sh [-p PROFILE] NAME < batch.txt" >&2; exit 2; }

# Run under the lock for the whole batch, so the console is ours until the
# .out is read. Inside our own train (token set) this is a plain exec.
[ -z "$LABLOCK_TOKEN" ] && exec sh "$here/lablock.sh" run "cfrun $1" -- sh "$0" -p "$profile" "$1"

LABCTL_WAIT=${CFRUN_WAIT:-600} exec sh "$here/labctl.sh" -p "$profile" send "$1"
