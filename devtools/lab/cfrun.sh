#!/bin/sh
# cfrun.sh: send one Command File Runner batch to the console and print its
# result (see SerialPrograms/Source/NintendoSwitch/Programs/CommandFileRunner/README.md).
#
#   cfrun.sh [-p PROFILE] NAME < batch.txt
#       Takes the console's lock (lablock.sh; fails at once if another train
#       holds it), puts the batch in the runner's inbox as NAME.txt, makes
#       sure the runner is running, waits for outbox/NAME.out and prints it.
#       Exit 0 when every line passed, 1 when a line failed or the runner
#       never took the file, 3 when the console is busy or locked, 4 when the
#       Command File Runner is not the selected program.
#
# PROFILE is the app's user-files folder (-p, else $LAB_PROFILE; see
# labctl.sh). The command folder is $LAB_CFR_DIR, else PROFILE/CommandFiles
# (the runner's default). Screenshots land in its outbox as NAME_label.png.
# $CFRUN_WAIT (default 600) is the longest a batch may take, in seconds.
#
# Rules it enforces, from the app's LabControl state file:
#   - a program other than the runner or Prevent Sleep is RUNNING: refuse,
#     touch nothing. Never stop a program to find out what runs.
#   - Prevent Sleep is RUNNING: it is the lab's own keep-awake; stop it so
#     the runner can take the console.
#   - the runner is selected but STOPPED: start it through labctl.sh.
#   - anything else is selected: the app has no way to select a program
#     from a script, so give up (exit 4) unless $CFRUN_SELECT is set to a
#     site-local command that gets the app to a running Command File Runner
#     (it is run with "sh -c").

here=$(dirname "$0")
profile=$LAB_PROFILE
if [ "$1" = -p ]; then profile=$2; shift 2; fi
[ -n "$profile" ] || { echo "cfrun: set LAB_PROFILE or pass -p PROFILE" >&2; exit 2; }
[ $# = 1 ] && [ -n "$1" ] || { echo "usage: cfrun.sh [-p PROFILE] NAME < batch.txt" >&2; exit 2; }
name=$1

# Run under the lock; the whole script, so the runner is ours until the
# .out is read. Inside our own train (token set) this is a plain exec.
[ -z "$LABLOCK_TOKEN" ] && exec sh "$here/lablock.sh" run "cfrun $name" -- sh "$0" -p "$profile" "$name"

CF=${LAB_CFR_DIR:-$profile/CommandFiles}
ST="$profile/LabControl/state"
CFR=NintendoSwitch:CommandFileRunner
KEEPAWAKE=NintendoSwitch:PreventSleep
W=${CFRUN_WAIT:-600}

state=$(sed -n 's/^state=//p' "$ST" 2>/dev/null)
prog=$(sed -n 's/^program=//p' "$ST" 2>/dev/null)
if [ "$state" = RUNNING ] && [ "$prog" != "$CFR" ] && [ "$prog" != "$KEEPAWAKE" ]; then
    echo "cfrun: the console is running $prog; not touching it." >&2; exit 3
fi

mkdir -p "$CF/inbox" "$CF/outbox" || exit 1
rm -f "$CF/outbox/$name.out" "$CF/outbox/$name.tmp"
# Write the batch outside the inbox, then move it in, so the runner never
# reads a half-written file.
cat > "$CF/$name.part" && mv "$CF/$name.part" "$CF/inbox/$name.txt" || exit 1
inbox="$CF/inbox/$name.txt"

if [ "$prog" = "$KEEPAWAKE" ] && [ "$state" = RUNNING ]; then
    sh "$here/labctl.sh" -p "$profile" stop >/dev/null || { rm -f "$inbox"; exit 1; }
    prog=$(sed -n 's/^program=//p' "$ST" 2>/dev/null); state=STOPPED
fi
if [ "$prog" = "$CFR" ]; then
    [ "$state" = RUNNING ] || sh "$here/labctl.sh" -p "$profile" start >/dev/null || { rm -f "$inbox"; exit 1; }
elif [ -n "$CFRUN_SELECT" ]; then
    sh -c "$CFRUN_SELECT" || { rm -f "$inbox"; exit 1; }
else
    rm -f "$inbox"
    echo "cfrun: the selected program is ${prog:-unknown}, not the Command File Runner; select it in the app first (or set CFRUN_SELECT)." >&2
    exit 4
fi

# The runner moves the file to done/ when it starts it. Fail fast if it never does.
i=0
while [ -f "$inbox" ]; do
    i=$((i + 1))
    [ $i -gt 90 ] && { rm -f "$inbox"; echo "cfrun: the runner did not take $name within 90s" >&2; exit 1; }
    sleep 1
done
# NAME.out appears only when the whole batch has finished.
end=$(( $(date +%s) + W ))
until [ -f "$CF/outbox/$name.out" ]; do
    [ "$(date +%s)" -ge "$end" ] && { echo "cfrun: no $name.out after ${W}s" >&2; exit 1; }
    sleep 1
done
cat "$CF/outbox/$name.out"
grep -q '^error line ' "$CF/outbox/$name.out" && exit 1
exit 0
