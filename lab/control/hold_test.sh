#!/bin/sh
# hold_test.sh : self-test of hold.sh and labctl.sh's refusal while held, in a temp profile.
# Touches nothing real (no app, no lab server: PA_LAB_CONFIG points nowhere). Run: sh lab/control/hold_test.sh
here=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/hold-test.XXXXXX); fail=0
trap 'rm -rf "$T"' EXIT
export LAB_PROFILE=$T/lab-T PA_LAB_CONFIG=$T/none.json LAB_DIR=$T/locks LAB_CONSOLE=T LABCTL_WAIT=1
mkdir -p "$LAB_PROFILE/Lab"
ok(){ if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }

ok "free at first" '[ "$(sh $here/hold.sh status)" = free ] && sh $here/hold.sh check'
ok "on" 'sh $here/hold.sh on | grep -q "^held by user since 20.*Z (hold_sh)$"'
ok "on writes a contract timestamp" 'grep -Eq "^acquired_at=[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:]{8}\.000Z$" "$LAB_PROFILE/Lab/hold"'
ok "on again keeps the first" 'sh $here/hold.sh on ui_start | grep -q "already held by user .*(hold_sh)"'
ok "check refuses with exit 4" 'sh $here/hold.sh check 2>/dev/null; [ $? = 4 ]'
ok "check says how to release" 'sh $here/hold.sh check 2>&1 | grep -q "hold.sh off releases it"'
ok "labctl start refused" 'sh $here/labctl.sh start 2>/dev/null; [ $? = 4 ]'
ok "labctl send refused" 'echo "press A" | sh $here/labctl.sh send x 2>/dev/null; [ $? = 4 ]'
ok "labctl keepawake on refused" 'sh $here/labctl.sh keepawake on 2>/dev/null; [ $? = 4 ]'
mkdir -p "$T/cfr/inbox" "$T/cfr/outbox"
ok "cfr.sh refused" 'echo "press A" | CFR_DIR=$T/cfr sh $here/../scripts/cfr.sh held 2>/dev/null; [ $? = 4 ]'
ok "cfr.sh queues nothing while held" '[ -z "$(ls "$T/cfr/inbox")" ]'
ok "labctl state still reads" 'printf "state=STOPPED\n" > "$LAB_PROFILE/Lab/state"; sh $here/labctl.sh state | grep -q STOPPED'
printf 'owner=user\r\nreason=user_input\r\nacquired_at=2026-11-15T21:04:05.120Z\r\n' > "$LAB_PROFILE/Lab/hold"
ok "reads the app's CRLF file" '[ "$(sh $here/hold.sh status)" = "held by user since 2026-11-15T21:04:05.120Z (user_input)" ]'
ok "off" 'sh $here/hold.sh off | grep -q "^free (was held by user"'
ok "free after off" '[ ! -e "$LAB_PROFILE/Lab/hold" ] && sh $here/hold.sh check'
ok "labctl start passes the hold (then finds no app)" 'sh $here/labctl.sh start 2>/dev/null; [ $? = 1 ]'
exit $fail
