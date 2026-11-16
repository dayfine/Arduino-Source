#!/bin/sh
# cfr.sh NAME < batch.txt : queue stdin as a Command File Runner batch in the lab app, select the
# CFR, start it, wait (max CFR_WAIT_S, default 600 s) for the batch to end, print its journal.
# CFR_DIR overrides the runner's folder (default <profile>/CommandFiles, with inbox/ and outbox/).
# Refuses (exit 4) while the user holds the console (hold.sh check), without queueing the batch.
# Syntax of the batch: README.md next to this script.
. "$(dirname "$0")/env.sh"
N=$1
[ -n "$N" ] || { echo "usage: cfr.sh NAME < batch.txt" >&2; exit 2; }
# The user's hold: refuse (exit 4) before queueing anything, or a CFR left running would press it,
# and a stopped one would run it at its next start.
sh "$LAB_CONTROL/hold.sh" check || exit 4
D=${CFR_DIR:-"$LAB_PROFILE/CommandFiles"}
OUT="$D/outbox/$N.out"
[ -f "$OUT" ] && mv "$OUT" "$OUT.prev"
cat > "$D/inbox/$N.part" && mv "$D/inbox/$N.part" "$D/inbox/$N.txt"
sh "$LAB_SCRIPTS/sel.sh" NintendoSwitch:CommandFileRunner || exit 1
LABCTL_WAIT=60 sh "$LAB_SCRIPTS/lc.sh" start >/dev/null 2>&1
if [ $? = 4 ]; then
  # The hold came on after the check: take the batch back out of the inbox.
  rm -f "$D/inbox/$N.txt"
  sh "$LAB_CONTROL/hold.sh" check
  exit 4
fi
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
