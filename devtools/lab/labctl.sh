#!/bin/sh
# labctl.sh: drive a running SerialPrograms app through its lab channel (see
# SerialPrograms/Source/CommonFramework/Windows/Lab.md). No window focus, no
# click. Every command is one request file; its journal is printed.
#
#   labctl.sh [-p PROFILE] state
#       Print the state file (seq=, state=, program=, owner=, error=).
#   labctl.sh [-p PROFILE] start|stop
#       Start or stop the selected program, then wait up to $LABCTL_WAIT
#       seconds (default 60) for the state to reach RUNNING (start) or
#       STOPPED (stop). Prints the final state; exit 0 on success, 1 otherwise.
#   labctl.sh [-p PROFILE] select GAME:PROGRAM
#       Show that program, as clicking it in the program list does.
#   labctl.sh [-p PROFILE] set KEY JSON...
#       Set one option of the selected program by its settings-file key, e.g.
#       set START_SEGMENT '"0"'  (the value is JSON: quote strings).
#   labctl.sh [-p PROFILE] owner claim TOKEN | owner release
#   labctl.sh [-p PROFILE] keepawake on|off [SECONDS]
#   labctl.sh [-p PROFILE] send NAME < request.txt
#       Send any request (one command per line) as NAME and print its journal
#       (outbox/NAME.out). Exit 1 if a line failed.
#   labctl.sh supports PATH
#       Exit 0 if the build at PATH (a build folder, .app bundle or binary)
#       has the lab channel.
#
# PROFILE is the app's user-files folder: the one holding SerialPrograms.log,
# Screenshots/ and the settings (-p, else $LAB_PROFILE). On macOS that is
# ~/Library/Application Support/SerialPrograms, or .../Profiles/NAME for an
# app launched with "--args --profile NAME"; on Windows and Linux it is the
# folder the app was started in.
#
# Once the channel is claimed (owner claim), every request that changes
# something must carry the token: set $LAB_OWNER and it is put on the first
# line of each request. Commands that drive the console (start, stop, select,
# set, keepawake, send) also refuse (exit 3) when another train holds the
# console's lock (lablock.sh check, console $LAB_CONSOLE).
# $LABCTL_WAIT (default 60) is also how long a request may take to run.

here=$(dirname "$0")

if [ "$1" = supports ]; then
    p=$2
    [ -d "$p" ] && [ -d "$p/SerialPrograms.app" ] && p=$p/SerialPrograms.app
    [ -d "$p" ] && [ -f "$p/Contents/MacOS/SerialPrograms" ] && p=$p/Contents/MacOS/SerialPrograms
    [ -d "$p" ] && [ -f "$p/SerialPrograms" ] && p=$p/SerialPrograms
    [ -d "$p" ] && [ -f "$p/SerialPrograms.exe" ] && p=$p/SerialPrograms.exe
    [ -f "$p" ] || { echo "labctl: no SerialPrograms binary at $2" >&2; exit 2; }
    grep -aq "Lab: running " "$p"; exit $?
fi

profile=$LAB_PROFILE
if [ "$1" = -p ]; then profile=$2; shift 2; fi
[ -n "$profile" ] || { echo "labctl: set LAB_PROFILE or pass -p PROFILE" >&2; exit 2; }
[ $# -ge 1 ] || { echo "usage: labctl.sh [-p PROFILE] state|start|stop|select ID|set KEY JSON|owner claim TOKEN|owner release|keepawake on|off [S]|send NAME" >&2; exit 2; }

D="$profile/Lab"
W=${LABCTL_WAIT:-60}
field(){ sed -n "s/^$1=//p" "$D/state" 2>/dev/null; }
nap(){ sleep 0.25 2>/dev/null || sleep 1; }
no_app(){ echo "labctl: no state file in $D (is a lab-channel build running with this profile, and does $D exist?)" >&2; exit 1; }

# request NAME: stdin is the request body; prints the journal, returns 1 if a
# line failed. The owner line goes first. Written beside the inbox and moved
# in, so the app never reads a half-written file.
request(){
    name=$1
    mkdir -p "$D/inbox" "$D/outbox" || exit 1
    rm -f "$D/outbox/$name.out" "$D/outbox/$name.tmp"
    { [ -n "$LAB_OWNER" ] && echo "owner $LAB_OWNER"; cat; } > "$D/$name.part" || exit 1
    mv "$D/$name.part" "$D/inbox/$name.txt" || exit 1
    end=$(( $(date +%s) + W ))
    until [ -f "$D/outbox/$name.out" ]; do
        if [ "$(date +%s)" -ge "$end" ]; then
            if [ -f "$D/inbox/$name.txt" ]; then
                rm -f "$D/inbox/$name.txt"; echo "labctl: the app did not take $name within ${W}s" >&2
            else
                echo "labctl: no $name.out after ${W}s" >&2
            fi
            return 1
        fi
        nap
    done
    cat "$D/outbox/$name.out"
    ! grep -q '^error ' "$D/outbox/$name.out"
}

cmd=$1; shift
case $cmd in
state)
    [ -f "$D/state" ] || no_app
    cat "$D/state"; exit 0;;
send)
    [ $# = 1 ] && [ -n "$1" ] || { echo "usage: labctl.sh [-p PROFILE] send NAME < request.txt" >&2; exit 2; }
    sh "$here/lablock.sh" check || exit 3
    request "$1"; exit $?;;
start|stop|select|set|owner|keepawake)
    sh "$here/lablock.sh" check || exit 3
    [ -f "$D/state" ] || mkdir -p "$D" || exit 1;;
*) echo "labctl: unknown command $cmd" >&2; exit 2;;
esac

name=labctl-$$-$(date +%s)
if [ "$cmd" = start ] || [ "$cmd" = stop ]; then
    [ "$cmd" = start ] && want=RUNNING || want=STOPPED
    # Wait until the app is polling and its program can take this command.
    end=$(( $(date +%s) + W ))
    while :; do
        s=$(field state)
        case "$cmd:$s" in
            start:STOPPED|stop:RUNNING) break;;
            start:RUNNING|stop:STOPPED) echo "already $s ($(field program))"; exit 0;;
            *:NO_PROGRAM) echo "the selected panel is not a program" >&2; exit 1;;
        esac
        [ "$(date +%s)" -ge "$end" ] && { echo "app not ready for $cmd (state: ${s:-no state file; is a lab-channel build running with this profile?})" >&2; exit 1; }
        nap
    done
    echo "$cmd" | request "$name" >/dev/null || { echo "$cmd failed: $(field error)" >&2; exit 1; }
    end=$(( $(date +%s) + W ))
    while :; do
        s=$(field state)
        [ "$s" = "$want" ] && { echo "$s ($(field program))"; exit 0; }
        [ "$(date +%s)" -ge "$end" ] && { echo "state is ${s:-unknown}, not $want, after ${W}s" >&2; exit 1; }
        nap
    done
fi

# select, set, owner, keepawake: one line, the words as given.
echo "$cmd $*" | request "$name"
