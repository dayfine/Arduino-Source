#!/bin/sh
# sel.sh PROGRAM : select PROGRAM in the lab app and wait until the controller is back
# ("Reading Controller Colors... Done" in the log). Retries once by selecting another program
# first; exit 1 if the controller never comes back.
# PROGRAM is the panel id, e.g. NintendoSwitch:CommandFileRunner or PokemonFRLG:AutoStory.
. "$(dirname "$0")/env.sh"
LOG="$LAB_PROFILE/SerialPrograms.log"
P=$1
[ -n "$P" ] || { echo "usage: sel.sh PROGRAM" >&2; exit 2; }
LABCTL_WAIT=60 sh "$LAB_SCRIPTS/lc.sh" state | grep -q "program=$P\$" && exit 0
wait_ready(){ # $1 = log byte offset before the select; 60 x 0.5 s
  i=0
  while [ $i -lt 60 ]; do
    tail -c +$(( $1 + 1 )) "$LOG" | LC_ALL=C grep -aq "Reading Controller Colors... Done" && return 0
    perl -e 'select(undef,undef,undef,0.5)'
    i=$((i + 1))
  done
  return 1
}
for try in 1 2; do
  off=$(stat -f %z "$LOG")
  LABCTL_WAIT=60 sh "$LAB_SCRIPTS/lc.sh" select "$P" >/dev/null 2>&1
  wait_ready "$off" && { echo "sel: $P ready"; exit 0; }
  echo "sel: controller not ready after selecting $P (try $try)" >&2
  other=NintendoSwitch:CommandFileRunner
  [ "$P" = "$other" ] && other=${SEL_FALLBACK:-PokemonFRLG:AutoStory}
  LABCTL_WAIT=60 sh "$LAB_SCRIPTS/lc.sh" select "$other" >/dev/null 2>&1
done
echo "sel: controller not connected" >&2
exit 1
