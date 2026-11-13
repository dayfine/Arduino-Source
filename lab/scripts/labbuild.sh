#!/bin/sh
# labbuild.sh GAME [--plan] : build and stage GAME's current clean tree in the background, no launch.
#   GAME = bdsp | frlg | plza. Log: ~/BDSPLab/build-GAME.log. Safe while another game's app runs
#   (start_dev.py --build-only takes no console lock). One build at a time machine-wide (-j4):
#   waits while any ninja runs and while another start_dev.py holds the build lock.
#   --plan prints what would run and touches nothing.
# Env: START_DEV (path of start_dev.py), CFG_DIR, LOGDIR, WAIT_MAX (checks of 30 s, default 360 = 3 h).
usage(){ echo "usage: labbuild.sh bdsp|frlg|plza [--plan]" >&2; exit 2; }
game=$1; [ -n "$game" ] || usage
CFG_DIR=${CFG_DIR:-$HOME/.config/pokemonautomation}; LOGDIR=${LOGDIR:-$HOME/BDSPLab}
START_DEV=${START_DEV:-$HOME/Projects/Arduino-Source-agents/lab/scripts/start_dev.py}
case $game in
  bdsp) cfg=$CFG_DIR/dev-labs.json ;;
  frlg) cfg=$CFG_DIR/dev-labs-frlg.json ;;
  plza) cfg=$CFG_DIR/dev-labs-plza.json ;;
  *) usage ;;
esac
log=$LOGDIR/build-$game.log
[ -f "$cfg" ] || { echo "labbuild: no config $cfg" >&2; exit 2; }
[ -f "$START_DEV" ] || { echo "labbuild: no $START_DEV" >&2; exit 2; }
grep -q -- '--build-only' "$START_DEV" || { echo "labbuild: $START_DEV lacks --build-only (update that checkout)" >&2; exit 2; }
if [ "$2" = --plan ]; then
  echo "would wait for ninja, then: python3 $START_DEV B --build-only --config $cfg > $log"
  exit 0
fi
[ -z "$2" ] || usage
WAIT_MAX=${WAIT_MAX:-360}
export START_DEV cfg log WAIT_MAX
nohup sh -c '
  say(){ echo "$(date +%T) labbuild: $*"; }
  : > "$log"
  exec >> "$log" 2>&1
  n=0
  while pgrep -x ninja >/dev/null; do
    n=$((n + 1)); [ $n -gt $WAIT_MAX ] && { say "gave up waiting for ninja"; echo "rc=3"; exit 3; }
    [ $n = 1 ] && say "another ninja is building; waiting"
    sleep 30
  done
  tries=0
  while :; do
    tries=$((tries + 1))
    say "build-only (try $tries)"
    out=$(PKG_CONFIG_PATH=/opt/homebrew/opt/opencv@4/lib/pkgconfig python3 "$START_DEV" B --build-only --config "$cfg" 2>&1); rc=$?
    printf "%s\n" "$out"
    case "$out" in
      *"Another dev launcher is building"*) [ $tries -lt $WAIT_MAX ] && { sleep 30; continue; } ;;
    esac
    break
  done
  say "done rc=$rc"; echo "rc=$rc"; exit $rc
' >/dev/null 2>&1 &
echo "labbuild $game started (pid $!), log $log"
