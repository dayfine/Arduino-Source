#!/bin/sh
# lablock.sh: one user of a lab console at a time (a "train").
#
#   lablock.sh [-c CONSOLE] run [-w] NAME -- CMD...
#       Take the console's lock, run CMD, release the lock when CMD exits.
#       -w waits in line for the lock; without it, fail at once if held.
#   lablock.sh [-c CONSOLE] check
#       Exit 0 if the caller may drive the console: the lock is free, or held
#       by the caller's own train (LABLOCK_TOKEN matches). Exit 3 otherwise.
#   lablock.sh [-c CONSOLE] status
#       Print who holds the lock.
#
# CONSOLE names the console (-c, else $LAB_CONSOLE, else "default"). The lock
# is the folder $LAB_DIR/locks/CONSOLE ($LAB_DIR defaults to ~/.lab).
#
# "run" exports LABLOCK_TOKEN to CMD. Scripts that act on the console
# (labctl.sh, cfrun.sh) call "check" first, so a stray script left over from
# an old run cannot stop or relaunch what the current run drives: only the
# process tree that took the lock carries its token. A lock whose holder has
# exited is stale and is cleared on the next call.

console=${LAB_CONSOLE:-default}
if [ "$1" = -c ]; then console=$2; shift 2; fi
[ -n "$console" ] || { echo "lablock: empty console name" >&2; exit 2; }
L=${LAB_DIR:-$HOME/.lab}/locks/$console
mkdir -p "$(dirname "$L")" || exit 2

# Publishing the lock is mkdir (atomic), writing its pid file is a second
# step. A lock without a pid is therefore either an owner still writing it
# or an owner that died in between; the age tells them apart.
INIT_GRACE=5
age(){ echo $(( $(date +%s) - $(stat -f %m "$1" 2>/dev/null || stat -c %Y "$1" 2>/dev/null || date +%s) )); }
held(){
    [ -d "$L" ] || return 1
    pid=$(cat "$L/pid" 2>/dev/null)
    if [ -n "$pid" ]; then kill -0 "$pid" 2>/dev/null; return $?; fi
    [ "$(age "$L")" -lt $INIT_GRACE ]
}
clear_stale(){ [ -d "$L" ] && ! held && rm -rf "$L"; }
holder(){ echo "$(cat "$L/name" 2>/dev/null) (pid $(cat "$L/pid" 2>/dev/null), since $(cat "$L/since" 2>/dev/null))"; }
own(){ [ -n "$LABLOCK_TOKEN" ] && [ -d "$L" ] && [ "$LABLOCK_TOKEN" = "$(cat "$L/token" 2>/dev/null)" ]; }

case $1 in
status)
    clear_stale
    if held; then echo "$console: held by $(holder)"; else echo "$console: free"; fi;;
check)
    clear_stale
    held || exit 0
    own && exit 0
    echo "$console is held by $(holder); refusing." >&2; exit 3;;
run)
    shift; wait=0
    [ "$1" = -w ] && { wait=1; shift; }
    name=$1; shift
    [ "$1" = -- ] && shift
    [ -n "$name" ] && [ $# -gt 0 ] || { echo "usage: lablock.sh [-c CONSOLE] run [-w] NAME -- CMD..." >&2; exit 2; }
    # Already inside our own train: just run.
    if own; then exec "$@"; fi
    while :; do
        clear_stale
        if mkdir "$L" 2>/dev/null; then break; fi
        [ $wait = 1 ] || { echo "$console is held by $(holder); not starting $name." >&2; exit 3; }
        sleep 2
    done
    tok=$$-$(date +%s)-$(od -An -N4 -tu4 /dev/urandom 2>/dev/null | tr -d ' ')
    echo "$tok" > "$L/token"; echo "$name" > "$L/name"; date '+%Y-%m-%d %H:%M:%S' > "$L/since"; echo $$ > "$L/pid"
    # Release only our own lock: a stale-cleared lock may have been retaken.
    trap '[ "$(cat "$L/token" 2>/dev/null)" = "$tok" ] && rm -rf "$L"' EXIT INT TERM HUP
    LABLOCK_TOKEN=$tok "$@"
    exit $?;;
*)
    echo "usage: lablock.sh [-c CONSOLE] run [-w] NAME -- CMD... | check | status" >&2; exit 2;;
esac
