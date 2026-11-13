#!/bin/sh
# labrun.sh GAME [N] [--plan] [--panel "Category" "Panel"] : launch the Nth newest staged lab-B build of GAME.
#   GAME = bdsp | frlg | plza; N = 1 (newest, default) .. 3.
#   Deployments come from ~/Projects/dev-deployments/{slot-B,slot-B-frlg,slot-B-plza} (made by
#   labbuild.sh / start_dev.py --build-only), ordered by their lab-deployment.json time.
#   Quits the running lab-B app first, but only when Lab/state says state=STOPPED (never a running
#   program; FORCE_PANEL=1 overrides, as in lab_panel.sh). No app running: nothing to quit.
#   Edits the panel selection while the app is closed. Env as lab_panel.sh: RUN_KEY=PokemonFRLG:AutoStory
#   RUN_SET='{"START_CHECKPOINT":39}' merges into that panel's RUN options.
#   Launches by direct exec (nohup), never `open` (open loses the camera permission), under the
#   console lock (lablock.sh, same lock start_dev.py takes); refuses if another train holds it.
#   --plan reports everything and changes nothing.
usage(){ echo "usage: labrun.sh bdsp|frlg|plza [N] [--plan] [--panel \"Category\" \"Panel\"]" >&2; exit 2; }
game=$1; [ -n "$game" ] || usage; shift
n=1; plan=; cat=; pan=
case $1 in [0-9]*) n=$1; shift ;; esac
while [ $# -gt 0 ]; do
  case $1 in
    --plan) plan=1; shift ;;
    --panel) [ $# -ge 3 ] || usage; cat=$2; pan=$3; shift 3 ;;
    *) usage ;;
  esac
done
case $game in bdsp) slot=slot-B ;; frlg) slot=slot-B-frlg ;; plza) slot=slot-B-plza ;; *) usage ;; esac
DEPLOYS=${DEPLOYS:-$HOME/Projects/dev-deployments}
PROFILES=${PROFILES:-$HOME/Library/Application Support/SerialPrograms/Profiles}
LABLOCK=${LABLOCK:-$HOME/BDSPLab/lablock.sh}
NAME=${LABRUN_PROFILE:-lab-B}; con=${NAME#lab-}   # LABRUN_PROFILE is for the self-test only
prof=$PROFILES/$NAME
# The app process itself: its command is [interpreter] .../SerialPrograms --profile NAME. A lablock or
# nohup wrapper has the same words later in its command line and must not match.
app_pid(){ ps -A -o pid= -o command= | awk -v n="$NAME" '{for(i=2;i<=3;i++) if($i ~ /\/SerialPrograms$/ && $(i+1)=="--profile" && $(i+2)==n){print $1; exit}}'; }
S=$prof/UserSettings/SerialPrograms-Settings.json
dir=$DEPLOYS/$slot
dep=$(ls -t "$dir"/*/lab-deployment.json 2>/dev/null | sed -n "${n}p" | xargs -I{} dirname {})
[ -n "$dep" ] || { echo "labrun: no deployment #$n in $dir (run labbuild.sh $game)" >&2; exit 2; }
bin=$dep/SerialPrograms.app/Contents/MacOS/SerialPrograms
[ -x "$bin" ] || { echo "labrun: no binary $bin" >&2; exit 2; }
rev=$(sed -n 's/.*"source_revision": *"\([0-9a-f]*\)".*/\1/p' "$dep/lab-deployment.json")
echo "labrun: $game #$n -> $(basename "$dep") commit ${rev:-?}"

pid=$(app_pid)
if [ -n "$pid" ]; then
  st=$(sed -n 's/^state=//p' "$prof/Lab/state" 2>/dev/null | head -1)
  if [ "$st" != STOPPED ] && [ -z "$FORCE_PANEL" ]; then
    echo "REFUSED: lab-B app is running (Lab/state=${st:-unknown}); not quitting it"; exit 2
  fi
fi
if [ -n "$plan" ]; then
  echo "plan: app running pid=${pid:-none}; would ${pid:+quit it, }${cat:+set panel '$cat / $pan', }${RUN_SET:+merge RUN_SET into $RUN_KEY, }take lablock -c $con, nohup exec:"
  echo "plan:   cd $dep && $bin --profile $NAME"
  sh "$LABLOCK" -c $con status 2>&1 | sed 's/^/plan: lock: /'
  exit 0
fi
if [ -n "$pid" ]; then
  kill -TERM "$pid"
  for i in $(seq 1 30); do [ -z "$(app_pid)" ] && break; sleep 1; done
  [ -n "$(app_pid)" ] && { echo "labrun: app did not quit in 30 s" >&2; exit 3; }
  for i in $(seq 1 30); do pgrep -f "start_dev.py $con" >/dev/null || break; sleep 1; done
fi
if [ -n "$cat$RUN_SET" ] && [ -f "$S" ]; then
  python3 - "$S" "$cat" "$pan" <<'PY'
import json,sys,os
p=sys.argv[1]; raw=open(p,'rb').read(); bom=raw.startswith(b'\xef\xbb\xbf'); d=json.loads(raw.decode('utf-8-sig'))
if sys.argv[2]:
    d['99-Panels']['ProgramCategory']=sys.argv[2]; d['99-Panels']['ProgramPanel']=sys.argv[3]
if os.environ.get('RUN_KEY') and os.environ.get('RUN_SET'):
    d['99-Panels'].setdefault(os.environ['RUN_KEY'],{}).setdefault('RUN',{}).update(json.loads(os.environ['RUN_SET']))
open(p,'wb').write((b'\xef\xbb\xbf' if bom else b'')+json.dumps(d,indent=4,ensure_ascii=False).encode())
PY
fi
out=${LOGDIR:-$HOME/BDSPLab}/labrun-$game.log
export LAB_CONSOLE=$con LAB_PROFILE_NAME=$NAME LAB_PROFILE="$prof"
( cd "$dep" && nohup sh "$LABLOCK" -c $con run "labrun-$game" -- "$bin" --profile $NAME > "$out" 2>&1 & )
for i in $(seq 1 20); do
  p=$(app_pid)
  [ -n "$p" ] && break
  sleep 1
done
if [ -z "$p" ]; then echo "labrun: NOT launched (console locked or crashed?):"; tail -5 "$out"; exit 3; fi
echo "labrun: launched $game pid $p commit ${rev:-?} deployment $dep"
[ -n "$cat" ] && echo "panel set: $cat / $pan"
exit 0
