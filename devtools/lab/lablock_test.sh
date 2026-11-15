#!/bin/sh
# lablock_test.sh: N callers race for one lock; exactly one must be admitted
# and their protected work must not overlap. Exit 0 when it holds.
#
#   lablock_test.sh [N]      (default 20)
here=$(dirname "$0")
N=${1:-20}
T=$(mktemp -d) || exit 2
export LAB_DIR=$T LAB_CONSOLE=race
for i in $(seq 1 "$N"); do
    sh "$here/lablock.sh" run "caller-$i" -- sh -c "echo start \$\$ >> '$T/trace'; sleep 2; echo end \$\$ >> '$T/trace'" >/dev/null 2>"$T/err.$i" &
done
wait
admitted=$(grep -c '^start' "$T/trace" 2>/dev/null || echo 0)
refused=$(cat "$T"/err.* 2>/dev/null | grep -c 'refusing\|not starting')
# Overlap: a second "start" before the first "end".
overlap=$(awk '$1=="start"{open++; if (open>1) o=1} $1=="end"{open--} END{print o+0}' "$T/trace" 2>/dev/null)
echo "callers=$N admitted=$admitted refused=$refused overlap=$overlap"
rm -rf "$T"
[ "$admitted" = 1 ] && [ "$overlap" = 0 ] && [ "$refused" = $((N - 1)) ]
