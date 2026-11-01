#!/bin/sh
# labctl_wait_test.sh: "labctl.sh wait" against result.json files written by
# hand: a run that ends while we wait, one that completed, one that never
# ends. Exit 0 when every check holds.
#
#   labctl_wait_test.sh
here=$(dirname "$0")
T=$(mktemp -d) || exit 2
export LAB_PROFILE=$T
fail=0
check(){ # check NAME EXPECTED ACTUAL
    if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1"; echo "  expected: $2"; echo "  actual:   $3"; fail=1; fi
}
r=20261101-153012123456-007
mkdir -p "$T/Lab/runs/$r"
printf '%s\n' '{' '    "event_count": 3,' '    "program": "PokemonBDSP:AutoStory",' "    \"run_id\": \"$r\"," '    "status": "running"' '}' > "$T/Lab/runs/$r/result.json"
# The run ends a second from now.
( sleep 1; cat > "$T/Lab/runs/$r/result.json.part" <<JSON
{
    "dropped_events": 0,
    "duration_ms": 650333,
    "end_time": "2026-11-01 15:41:02.456",
    "error": "expect_failed: the Route 204 \"sign\" box did not open",
    "event_count": 42,
    "frame": "frame-failure.png",
    "last_event": {
        "attempt": 2,
        "checkpoint": "01.08",
        "frame": "frame-041-01.08-s4_7-a2.png",
        "kind": "attempt",
        "pose": "route_204_south (12, 41) facing N",
        "step": "4/7",
        "text": "the sign's box did not open",
        "time": "2026-11-01 15:41:02.400"
    },
    "log_truncated": false,
    "outcome": "failed",
    "program": "PokemonBDSP:AutoStory",
    "run_id": "$r",
    "start_time": "2026-11-01 15:30:12.123",
    "status": "terminal"
}
JSON
mv "$T/Lab/runs/$r/result.json.part" "$T/Lab/runs/$r/result.json" ) &
out=$(LABCTL_RUN_WAIT=10 sh "$here/labctl.sh" wait $r); rc=$?
wait
check "failed run: exit 1" 1 "$rc"
check "failed run: output" "outcome=failed  checkpoint=01.08  step=4/7  attempt=2
error: expect_failed: the Route 204 \\\"sign\\\" box did not open
frame: $T/Lab/runs/$r/frame-failure.png
pose: route_204_south (12, 41) facing N
events: $T/Lab/runs/$r/events.jsonl   log: $T/Lab/runs/$r/log.txt" "$out"

c=20261101-153012123456-008
mkdir -p "$T/Lab/runs/$c"
printf '%s\n' '{' '    "event_count": 0,' '    "outcome": "completed",' "    \"run_id\": \"$c\"," '    "status": "terminal"' '}' > "$T/Lab/runs/$c/result.json"
out=$(sh "$here/labctl.sh" wait $c); rc=$?
check "completed run: exit 0" 0 "$rc"
check "completed run: output" "outcome=completed
events: $T/Lab/runs/$c/events.jsonl   log: $T/Lab/runs/$c/log.txt" "$out"

s=20261101-153012123456-009
mkdir -p "$T/Lab/runs/$s"
printf '%s\n' '{' '    "status": "running"' '}' > "$T/Lab/runs/$s/result.json"
err=$(LABCTL_RUN_WAIT=1 sh "$here/labctl.sh" wait $s 2>&1 >/dev/null); rc=$?
check "still running: exit 2" 2 "$rc"
check "still running: message" "labctl: run $s is still running after 1s" "$err"

err=$(LABCTL_RUN_WAIT=1 sh "$here/labctl.sh" wait nobody 2>&1 >/dev/null); rc=$?
check "unknown run: exit 2" 2 "$rc"
rm -rf "$T"
exit $fail
