#!/bin/sh
# SUPERSEDED by lab/scripts/wait_for.sh: use `sh lab/scripts/wait_for.sh --max-minutes 60 checks <PR>`.
# Kept for old notes that call it by run id.
# ciwait.sh RUN_ID... : wait (max CI_WAIT_MIN, default 60 min) for GitHub Actions runs on the fork,
# then print branch / workflow / conclusion and up to five failed test names per run.
# Exit 0 when all runs completed, 1 when some were still running at the cap.
REPO=${CI_REPO:-dayfine/Arduino-Source}
cap=${CI_WAIT_MIN:-60}
[ $# -gt 0 ] || { echo "usage: ciwait.sh RUN_ID..." >&2; exit 2; }
i=0
left=$#
while [ $i -lt "$cap" ]; do
  left=0
  for r in "$@"; do
    st=$(gh run view "$r" --repo "$REPO" --json status --jq .status 2>/dev/null)
    [ "$st" = completed ] || left=$((left + 1))
  done
  [ $left = 0 ] && break
  sleep 60
  i=$((i + 1))
done
for r in "$@"; do
  gh run view "$r" --repo "$REPO" --json headBranch,name,conclusion --jq '"\(.headBranch)\t\(.name)\t\(.conclusion)"'
  gh run view "$r" --repo "$REPO" --log-failed 2>/dev/null | grep -ao "Failed: [A-Za-z:]* - [^,]*" | sort -u | head -5
done
[ $left = 0 ]
