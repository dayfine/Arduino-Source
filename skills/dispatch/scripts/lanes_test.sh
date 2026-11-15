#!/bin/sh
# lanes_test.sh: tests for lanes.sh against a temporary HOME (never the live board).
# Exit 0 when every case holds.
#   1. show prints every lane with its minutes in state.
#   2. stalled lists idle, stale waiting-ci and old waiting-user lanes, skips fresh ones, exits 1.
#   3. stalled exits 0 when every lane is moving.
#   4. set state stamps since (the lane drops out of stalled).
#   5. set on an unknown lane exits 3 and leaves the board unchanged.
here=$(cd "$(dirname "$0")" && pwd)
LANES=${LANES_SH:-$here/lanes.sh}
T=$(mktemp -d) || exit 2
trap 'rm -rf "$T"' EXIT
export HOME=$T
B=$T/BDSPLab/dispatch; mkdir -p "$B"
fail=0
verdict(){ # name rc details
    if [ "$2" = 0 ]; then echo "ok   $1: $3"; else echo "FAIL $1: $3"; fail=1; fi
}
# Stamp N minutes ago, BSD or GNU date.
ago(){ s=$(( $(date +%s) - $1 * 60 ))
    date -r "$s" '+%Y-%m-%d %H:%M' 2>/dev/null || date -d "@$s" '+%Y-%m-%d %H:%M'; }
board(){ # rows on stdin, after the template header
    cp "$here/../templates/lanes.tsv" "$B/lanes.tsv"; cat >> "$B/lanes.tsv"; }
tab=$(printf '\t')

board <<R
FRLG${tab}idle${tab}-${tab}$(ago 5)${tab}rerun 64${tab}-
PLZA${tab}waiting-ci${tab}agent${tab}$(ago 45)${tab}merge #1${tab}CI 111
seams${tab}waiting-ci${tab}agent${tab}$(ago 3)${tab}merge #2${tab}CI 222
MCP${tab}waiting-user${tab}-${tab}$(ago 150)${tab}slice 5${tab}which port
mGBA${tab}waiting-user${tab}-${tab}$(ago 20)${tab}bridge${tab}lua version
BDSP${tab}moving${tab}agent${tab}$(ago 50)${tab}run 3${tab}-
R

# 1. show
out=$(sh "$LANES" show); rc=$?
n=$(printf '%s\n' "$out" | grep -cE '^(FRLG|PLZA|seams|MCP|mGBA|BDSP) ')
printf '%s\n' "$out" | grep -qE '^PLZA +waiting-ci +agent +4[56] '
verdict show $(( rc + (n != 6) + $? )) "6 lanes, PLZA 45 min (rc=$rc, rows=$n)"

# 2. stalled with idle / stale-ci / fresh / waiting-user rows
out=$(sh "$LANES" stalled); rc=$?
bad=0
printf '%s\n' "$out" | grep -q '^IDLE FRLG -> rerun 64$' || bad=1
printf '%s\n' "$out" | grep -q '^STALE PLZA (waiting-ci 4[56]m: CI 111) -> merge #1$' || bad=1
printf '%s\n' "$out" | grep -q '^ASK MCP (15[01]m: which port)$' || bad=1
printf '%s\n' "$out" | grep -qE 'seams|mGBA|BDSP' && bad=1
verdict stalled $(( (rc != 1) + bad )) "idle, stale-ci, ask listed; fresh ci, fresh user, moving skipped (rc=$rc)"

# 3. stalled exits 0 when all lanes move
board <<R
FRLG${tab}moving${tab}console${tab}$(ago 90)${tab}run 65${tab}-
R
out=$(sh "$LANES" stalled); rc=$?
printf '%s\n' "$out" | grep -q 'all lanes moving'
verdict all-moving $(( rc + $? )) "rc=$rc"

# 4. set state stamps since
board <<R
PLZA${tab}waiting-ci${tab}agent${tab}$(ago 45)${tab}merge #1${tab}CI 111
R
sh "$LANES" set PLZA state moving >/dev/null; rc=$?
since=$(awk -F'\t' '$1=="PLZA"{print $2 "|" $4}' "$B/lanes.tsv")
now=$(date '+%Y-%m-%d %H:%M'); prev=$(ago 1)
case $since in "moving|$now"|"moving|$prev") ok=0 ;; *) ok=1 ;; esac
sh "$LANES" set PLZA next "author 03.10" >/dev/null
nx=$(awk -F'\t' '$1=="PLZA"{print $5 "|" $4}' "$B/lanes.tsv")
case $nx in "author 03.10|$now"|"author 03.10|$prev") ;; *) ok=1 ;; esac
verdict set-since $(( rc + ok )) "state+since '$since', next '$nx'"

# 5. unknown lane
cp "$B/lanes.tsv" "$T/before"
sh "$LANES" set Nope state idle >/dev/null 2>&1; rc=$?
cmp -s "$B/lanes.tsv" "$T/before"; same=$?
[ -e "$B/lanes.tsv.tmp" ]; tmp=$((1 - $?))
verdict unknown-lane $(( (rc != 3) + same + tmp )) "rc=$rc, board unchanged=$((1 - same))"

exit $fail
