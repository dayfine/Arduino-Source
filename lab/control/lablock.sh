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
# is $LAB_DIR/locks/CONSOLE ($LAB_DIR defaults to ~/.lab); $L/token, $L/pid,
# $L/name and $L/since describe the holder.
#
# "run" exports LABLOCK_TOKEN to CMD. Scripts that act on the console
# (labctl.sh, cfrun.sh) call "check" first, so a stray script left over from
# an old run cannot stop or relaunch what the current run drives: only the
# process tree that took the lock carries its token. A lock whose holder has
# exited is stale and is cleared on the next call.
#
# The lock is a symlink CONSOLE -> .CONSOLE.TOKEN, a folder filled in before
# the link is made, so nobody ever sees a lock without its pid. Clearing a
# stale lock happens under a second lock, CONSOLE.clearing (a symlink to the
# clearer's pid): a lock can only disappear by its owner's release or by a
# clearer holding that marker, so a clearer that re-judges the lock under the
# marker removes exactly the stale lock it judged, never one retaken since.
# A plain folder CONSOLE is the older format (mkdir, then the files); it is
# still honoured and cleared.

console=${LAB_CONSOLE:-default}
if [ "$1" = -c ]; then console=$2; shift 2; fi
case $console in ''|*/*|.*) echo "lablock: bad console name '$console'" >&2; exit 2;; esac
LOCKS=${LAB_DIR:-$HOME/.lab}/locks
L=$LOCKS/$console
M=$L.clearing
mkdir -p "$LOCKS" || exit 2

# Older-format locks publish the folder first and the pid second; a folder
# without a pid younger than this is an owner still writing it.
INIT_GRACE=5
age(){ echo $(( $(date +%s) - $(stat -c %Y "$1" 2>/dev/null || stat -f %m "$1" 2>/dev/null || date +%s) )); }
alive(){ [ -n "$1" ] && kill -0 "$1" 2>/dev/null; }
held(){
    if [ -L "$L" ]; then
        alive "$(cat "$L/pid" 2>/dev/null)"; return
    fi
    [ -d "$L" ] || return 1
    pid=$(cat "$L/pid" 2>/dev/null)
    [ -n "$pid" ] && { alive "$pid"; return; }
    [ "$(age "$L")" -lt $INIT_GRACE ]
}
remove_lock(){
    if [ -L "$L" ]; then
        t=$(readlink "$L")
        rm -f "$L"
        case $t in ".$console".*/*|"") ;; ".$console".*) rm -rf "${LOCKS:?}/$t";; esac
    elif [ -d "$L" ]; then
        rm -rf "$L"
    fi
}
clear_stale(){
    { [ -L "$L" ] || [ -d "$L" ]; } && ! held || return 0
    if ! ln -s "$$" "$M" 2>/dev/null; then
        c=$(readlink "$M" 2>/dev/null)
        [ -n "$c" ] && ! alive "$c" &&
            echo "lablock: a stale-lock clear by pid $c was interrupted; once no lablock.sh runs, remove $M" >&2
        return 0
    fi
    held || remove_lock
    rm -f "$M"
}
holder(){ echo "$(cat "$L/name" 2>/dev/null) (pid $(cat "$L/pid" 2>/dev/null), since $(cat "$L/since" 2>/dev/null))"; }
own(){ [ -n "$LABLOCK_TOKEN" ] && [ "$LABLOCK_TOKEN" = "$(cat "$L/token" 2>/dev/null)" ]; }

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
    tok=$$-$(date +%s)-$(od -An -N4 -tu4 /dev/urandom 2>/dev/null | tr -d ' ')
    me=.$console.$tok
    mkdir "$LOCKS/$me" || exit 2
    echo "$tok" > "$LOCKS/$me/token"; echo "$name" > "$LOCKS/$me/name"
    date '+%Y-%m-%d %H:%M:%S' > "$LOCKS/$me/since"; echo $$ > "$LOCKS/$me/pid"
    # Release only our own lock. While we live nobody else removes it.
    trap '[ "$(readlink "$L" 2>/dev/null)" = "$me" ] && rm -f "$L"; rm -rf "${LOCKS:?}/$me"' EXIT INT TERM HUP
    while :; do
        clear_stale
        # ln -n fails on an existing lock link; on an older-format folder it
        # would put the link inside it, so confirm the link is the lock.
        if ln -sn "$me" "$L" 2>/dev/null; then
            [ "$(readlink "$L" 2>/dev/null)" = "$me" ] && break
            rm -f "$L/$me"
        fi
        [ $wait = 1 ] || { echo "$console is held by $(holder); not starting $name." >&2; exit 3; }
        sleep 2
    done
    LABLOCK_TOKEN=$tok "$@"
    exit $?;;
*)
    echo "usage: lablock.sh [-c CONSOLE] run [-w] NAME -- CMD... | check | status" >&2; exit 2;;
esac
