#!/bin/sh
# labctl.sh: start or stop the selected program of a running SerialPrograms
# app, or read its state, through the app's LabControl folder (see
# SerialPrograms/Source/CommonFramework/Windows/LabControl.md). No window
# focus, no click.
#
#   labctl.sh [-p PROFILE] start|stop
#       Write the command file, then wait up to $LABCTL_WAIT seconds (default
#       60) for the app's state file to reach RUNNING (start) or STOPPED
#       (stop). Prints the final state; exit 0 on success, 1 otherwise.
#       Refuses (exit 3) when another train holds the console's lock
#       (lablock.sh check).
#   labctl.sh [-p PROFILE] state
#       Print the state file (state=, program=, error=).
#   labctl.sh supports PATH
#       Exit 0 if the build at PATH (a build folder, .app bundle or binary)
#       has LabControl.
#
# PROFILE is the app's user-files folder: the one holding SerialPrograms.log,
# Screenshots/ and the settings (-p, else $LAB_PROFILE). On macOS that is
# ~/Library/Application Support/SerialPrograms, or .../Profiles/NAME for an
# app launched with "--args --profile NAME"; on Windows and Linux it is the
# folder the app was started in. The console whose lock is checked is
# $LAB_CONSOLE (see lablock.sh).

here=$(dirname "$0")

if [ "$1" = supports ]; then
    p=$2
    [ -d "$p" ] && [ -d "$p/SerialPrograms.app" ] && p=$p/SerialPrograms.app
    [ -d "$p" ] && [ -f "$p/Contents/MacOS/SerialPrograms" ] && p=$p/Contents/MacOS/SerialPrograms
    [ -d "$p" ] && [ -f "$p/SerialPrograms" ] && p=$p/SerialPrograms
    [ -d "$p" ] && [ -f "$p/SerialPrograms.exe" ] && p=$p/SerialPrograms.exe
    [ -f "$p" ] || { echo "labctl: no SerialPrograms binary at $2" >&2; exit 2; }
    grep -aq "LabControl: " "$p"; exit $?
fi

profile=$LAB_PROFILE
if [ "$1" = -p ]; then profile=$2; shift 2; fi
[ -n "$profile" ] || { echo "labctl: set LAB_PROFILE or pass -p PROFILE" >&2; exit 2; }
[ $# = 1 ] || { echo "usage: labctl.sh [-p PROFILE] start|stop|state | labctl.sh supports PATH" >&2; exit 2; }

D="$profile/LabControl"
field(){ sed -n "s/^$1=//p" "$D/state" 2>/dev/null; }
nap(){ sleep 0.25 2>/dev/null || sleep 1; }

case $1 in
state)
    [ -f "$D/state" ] || { echo "no state file in $D (is a LabControl build running with this profile?)" >&2; exit 1; }
    cat "$D/state"; exit 0;;
start) want=RUNNING;;
stop)  want=STOPPED;;
*) echo "labctl: unknown command $1" >&2; exit 2;;
esac
cmd=$1

# One train on the console: refuse if another run holds the lock.
sh "$here/lablock.sh" check || exit 3

W=${LABCTL_WAIT:-60}
mkdir -p "$D" || exit 1
# A state file left by an earlier app session may be stale; a running app
# rewrites it on its next poll.
rm -f "$D/state"
end=$(( $(date +%s) + W ))

# Wait until the app is polling and its program can take this command.
while :; do
    s=$(field state)
    case "$cmd:$s" in
        start:STOPPED|stop:RUNNING) break;;
        start:RUNNING|stop:STOPPED) echo "already $s ($(field program))"; exit 0;;
        *:NO_PROGRAM) echo "the selected panel is not a program" >&2; exit 1;;
    esac
    [ "$(date +%s)" -ge "$end" ] && { echo "app not ready for $cmd (state: ${s:-no state file; is a LabControl build running with this profile?})" >&2; exit 1; }
    nap
done

# Drop the command atomically: only the exact name acts, so the app never
# reads a half-written file.
: > "$D/$cmd.part" && mv "$D/$cmd.part" "$D/$cmd"
while [ -e "$D/$cmd" ]; do
    [ "$(date +%s)" -ge "$end" ] && { rm -f "$D/$cmd"; echo "app did not pick up $cmd" >&2; exit 1; }
    nap
done

# The app writes the state right after acting on the file; give it one poll.
sleep 0.6 2>/dev/null || sleep 1
while :; do
    s=$(field state); e=$(field error)
    [ -n "$e" ] && { echo "$cmd failed: $e (state $s)" >&2; exit 1; }
    [ "$s" = "$want" ] && { echo "$s ($(field program))"; exit 0; }
    [ "$(date +%s)" -ge "$end" ] && { echo "state is ${s:-unknown}, not $want, after ${W}s" >&2; exit 1; }
    nap
done
