#!/bin/sh
# cfr.sh NAME < batch.txt : queue stdin as a Command File Runner batch in the lab app, select the
# CFR, start it, wait (max CFR_WAIT_S, default 600 s) for the batch to end, print its journal.
# CFR_DIR overrides the runner's folder (default <profile>/CommandFiles, with inbox/ and outbox/).
# Syntax of the batch: README.md next to this script.
. "$(dirname "$0")/env.sh"
N=$1
[ -n "$N" ] || { echo "usage: cfr.sh NAME < batch.txt" >&2; exit 2; }
D=${CFR_DIR:-"$LAB_PROFILE/CommandFiles"}
OUT="$D/outbox/$N.out"
[ -f "$OUT" ] && mv "$OUT" "$OUT.prev"
cat > "$D/inbox/$N.part" && mv "$D/inbox/$N.part" "$D/inbox/$N.txt"
sh "$LAB_SCRIPTS/sel.sh" NintendoSwitch:CommandFileRunner || exit 1
LABCTL_WAIT=60 sh "$LAB_SCRIPTS/lc.sh" start >/dev/null 2>&1
cap=$(( ${CFR_WAIT_S:-600} * 2 ))
i=0
while [ $i -lt $cap ]; do
  [ -f "$OUT" ] && tail -1 "$OUT" | grep -q '^end' && break
  pgrep -f -- "--profile $LAB_PROFILE_NAME" >/dev/null || { echo "cfr.sh: lab app ($LAB_PROFILE_NAME) is not running" >&2; break; }
  perl -e 'select(undef,undef,undef,0.5)'
  i=$((i + 1))
done
[ $i -ge $cap ] && echo "(no result within ${CFR_WAIT_S:-600} s)"
cat "$OUT" 2>/dev/null
