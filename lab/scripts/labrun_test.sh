#!/bin/sh
# labrun_test.sh : plan-only self-test of labrun.sh (--plan never quits or launches anything) against
# fake deployments and a private lock root. Touches nothing real: profile "lab-T", console T, $T/ folders. Run: sh lab/scripts/labrun_test.sh
here=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/labrun-test.XXXXXX); fail=0
LOCKSRC=$here/../control/lablock.sh
[ -f "$LOCKSRC" ] || LOCKSRC=$HOME/BDSPLab/lablock.sh
trap 'rm -rf "$T"' EXIT
export DEPLOYS=$T/dep PROFILES=$T/prof LABLOCK=$LOCKSRC LAB_DIR=$T/lab LOGDIR=$T LABRUN_PROFILE=lab-T
ok(){ if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }
mk(){ # mk SLOT NAME REV AGE
  d=$DEPLOYS/$1/$2; mkdir -p "$d/SerialPrograms.app/Contents/MacOS"
  printf '{\n  "source_revision": "%s"\n}\n' "$3" > "$d/lab-deployment.json"
  printf '#!/bin/sh\ntrap "exit 0" TERM\nsleep 60 &\nwait\n' > "$d/SerialPrograms.app/Contents/MacOS/SerialPrograms"
  chmod +x "$d/SerialPrograms.app/Contents/MacOS/SerialPrograms"; touch -t "$4" "$d/lab-deployment.json"; }
mk slot-B-frlg old-1 aaaaaaaaaaaa 202601010101; mk slot-B-frlg new-2 bbbbbbbbbbbb 202601010202
mk slot-B d-1 cccccccccccc 202601010303
mkdir -p "$PROFILES/lab-T/Lab" "$PROFILES/lab-T/UserSettings"
printf 'state=STOPPED\n' > "$PROFILES/lab-T/Lab/state"
printf '{"99-Panels":{"ProgramCategory":"x","ProgramPanel":"y"}}' > "$PROFILES/lab-T/UserSettings/SerialPrograms-Settings.json"
S=$PROFILES/lab-T/UserSettings/SerialPrograms-Settings.json

ok "plan names newest" 'sh $here/labrun.sh frlg --plan | grep -q "new-2 commit bbbbbbbbbbbb"'
ok "plan N=2 names older" 'sh $here/labrun.sh frlg 2 --plan | grep -q "old-1 commit aaaa"'
ok "plan N=3 refused" '! sh $here/labrun.sh frlg 3 --plan 2>/dev/null'
ok "bdsp picks slot-B" 'sh $here/labrun.sh bdsp --plan | grep -q "d-1 commit cccc"'

exit $fail
