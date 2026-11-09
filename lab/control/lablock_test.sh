#!/bin/sh
# lablock_test.sh: race tests for lablock.sh, each in a temporary lock folder
# (never the live ~/.lab). Exit 0 when every case holds.
#
#   lablock_test.sh [N]      (default 20 callers; LABLOCK_TEST_ROUNDS rounds, default 3)
#
# LABLOCK_SH picks the script under test (default: lablock.sh next to this).
#  1. contention: N callers race; the winner holds until every other caller
#     has given up, so exactly one is admitted however slow the machine is.
#  2. queue: callers with -w are all admitted, one at a time.
#  3. clear vs retake: a caller judges an old lock stale and stalls before
#     removing it; another clears it and takes the console meanwhile. The
#     stalled clear must not remove the new holder's lock.
#  4. half-written lock: a caller stalls while taking the lock and another
#     caller's clock reads 10 s later. It must not judge the lock abandoned.
here=$(cd "$(dirname "$0")" && pwd)
N=${1:-20}
ROUNDS=${LABLOCK_TEST_ROUNDS:-3}
LOCK=${LABLOCK_SH:-$here/lablock.sh}
T=$(mktemp -d) || exit 2
trap 'rm -rf "$T"' EXIT
export LAB_CONSOLE=race
unset LABLOCK_TOKEN
fail=0

# Protected work, written to DIR/work.sh: log start/end, hold until DIR/done
# has NEED lines (at most 30 s), then HOLD seconds more.
work(){ # DIR NEED HOLD [TRACE]
    cat > "$1/work.sh" <<W
echo start \$\$ >> "${4:-$1/trace}"
i=0
while [ \$(cat "$1/done" 2>/dev/null | wc -l) -lt $2 ] && [ \$i -lt 300 ]; do sleep 0.1; i=\$((i + 1)); done
sleep $3
echo end \$\$ >> "${4:-$1/trace}"
W
}
overlap(){ awk '$1=="start"{open++; if (open>1) o=1} $1=="end"{open--} END{print o+0}' "$1/trace" 2>/dev/null; }
admitted(){ grep -c '^start' "$1/trace" 2>/dev/null || echo 0; }
# Wait (max 10 s) for a file to appear.
await(){ i=0; while [ ! -e "$1" ] && [ $i -lt 100 ]; do sleep 0.1; i=$((i+1)); done; }
verdict(){ # name ok-condition-result details
    if [ "$2" = 0 ]; then echo "ok   $1: $3"; else echo "FAIL $1: $3"; fail=1; fi
}

# Shims for cases 3 and 4: a slow rm (stalls once marked), a slow od, a clock 10 s ahead.
S=$T/shims; mkdir -p "$S/slowrm" "$S/slowod" "$S/later"
REAL_RM=$(command -v rm) REAL_OD=$(command -v od) REAL_DATE=$(command -v date)
printf '#!/bin/sh\n[ -e "$SHIM_MARK" ] || { : > "$SHIM_MARK"; sleep 2; }\nexec %s "$@"\n' "$REAL_RM" > "$S/slowrm/rm"
printf '#!/bin/sh\n: > "$SHIM_MARK"; sleep 2\nexec %s "$@"\n' "$REAL_OD" > "$S/slowod/od"
printf '#!/bin/sh\nif [ "$1" = +%%s ]; then echo $(( $(%s +%%s) + 10 )); else exec %s "$@"; fi\n' "$REAL_DATE" "$REAL_DATE" > "$S/later/date"
chmod +x "$S/slowrm/rm" "$S/slowod/od" "$S/later/date"

# 1. contention
r=1
while [ $r -le "$ROUNDS" ]; do
    C=$T/c$r; mkdir -p "$C"; export LAB_DIR=$C/lab; work "$C" $((N - 1)) 0
    for i in $(seq 1 "$N"); do
        { sh "$LOCK" run "caller-$i" -- sh "$C/work.sh" >/dev/null 2>"$C/err.$i"; echo "$i" >> "$C/done"; } &
    done
    wait
    a=$(admitted "$C"); o=$(overlap "$C"); f=$(cat "$C"/err.* | grep -c 'not starting')
    [ "$a" = 1 ] && [ "$o" = 0 ] && [ "$f" = $((N - 1)) ]
    verdict "contention round $r" $? "callers=$N admitted=$a refused=$f overlap=$o"
    r=$((r + 1))
done

# 2. queue
Q=5; C=$T/q; mkdir -p "$C"; export LAB_DIR=$C/lab; work "$C" 0 0.2
for i in $(seq 1 $Q); do
    sh "$LOCK" run -w "queued-$i" -- sh "$C/work.sh" >/dev/null 2>"$C/err.$i" &
done
wait
a=$(admitted "$C"); o=$(overlap "$C")
[ "$a" = $Q ] && [ "$o" = 0 ]
verdict "queue" $? "callers=$Q admitted=$a overlap=$o"

# A pid that is certainly gone.
sh -c 'exit 0' & dead=$!; wait

# 3. clear vs retake
C=$T/s; mkdir -p "$C/lab/locks/race"; export LAB_DIR=$C/lab
echo "$dead" > "$C/lab/locks/race/pid"; echo old > "$C/lab/locks/race/name"
mkdir "$C/a" "$C/b"; work "$C/a" 0 1 "$C/trace"; work "$C/b" 0 3 "$C/trace"
{ PATH=$S/slowrm:$PATH SHIM_MARK=$C/mark sh "$LOCK" run stalled-clear -- sh "$C/a/work.sh" >/dev/null 2>"$C/err.1"; echo 1 >> "$C/done"; } &
await "$C/mark"
{ sh "$LOCK" run retake -- sh "$C/b/work.sh" >/dev/null 2>"$C/err.2"; echo 2 >> "$C/done"; } &
wait
a=$(admitted "$C"); o=$(overlap "$C")
[ "$a" = 1 ] && [ "$o" = 0 ]
verdict "clear vs retake" $? "admitted=$a overlap=$o"

# 4. half-written lock
C=$T/h; mkdir -p "$C/a" "$C/b"; export LAB_DIR=$C/lab; work "$C/a" 0 1 "$C/trace"; work "$C/b" 0 3 "$C/trace"
{ PATH=$S/slowod:$PATH SHIM_MARK=$C/mark sh "$LOCK" run slow-taker -- sh "$C/a/work.sh" >/dev/null 2>"$C/err.1"; echo 1 >> "$C/done"; } &
await "$C/mark"
{ PATH=$S/later:$PATH sh "$LOCK" run clock-ahead -- sh "$C/b/work.sh" >/dev/null 2>"$C/err.2"; echo 2 >> "$C/done"; } &
wait
a=$(admitted "$C"); o=$(overlap "$C")
[ "$a" = 1 ] && [ "$o" = 0 ]
verdict "half-written lock" $? "admitted=$a overlap=$o"

exit $fail
