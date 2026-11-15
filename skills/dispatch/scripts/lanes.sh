#!/bin/sh
# lanes.sh show | stalled | set LANE FIELD VALUE
# One row per lane in ~/BDSPLab/dispatch/lanes.tsv:
#   lane<TAB>state<TAB>owner<TAB>since<TAB>next<TAB>wait_on
# state: moving (agent/console working) | waiting-ci | waiting-user | waiting-console | idle | paused
# stalled = idle, or waiting-ci/waiting-console older than 30 min, or waiting-user older than 2 h.
# Exit 1 from 'stalled' when any lane needs action.
F="$HOME/BDSPLab/dispatch/lanes.tsv"
now=$(date +%s)
# BSD date (macOS), then GNU date (CI); an unreadable stamp counts as now.
age(){ s=$(date -j -f '%Y-%m-%d %H:%M' "$1" +%s 2>/dev/null || date -d "$1" +%s 2>/dev/null || echo "$now"); echo $(( (now - s) / 60 )); }
rows(){ tail -n +2 "$F" | while IFS="$(printf '\t')" read -r l st o since nx w; do
  a=$(age "$since")
  case $st in
    idle) echo "IDLE $l -> $nx" ;;
    waiting-ci|waiting-console) [ "$a" -ge 30 ] && echo "STALE $l ($st ${a}m: $w) -> $nx" ;;
    waiting-user) [ "$a" -ge 120 ] && echo "ASK $l (${a}m: $w)" ;;
  esac; done; }
case $1 in
  show)
    printf 'LANE\tSTATE\tOWNER\tMIN\tNEXT\tWAIT_ON\n' > "$F.view"
    tail -n +2 "$F" | while IFS="$(printf '\t')" read -r l st o since nx w; do
      printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$l" "$st" "$o" "$(age "$since")" "$nx" "$w"; done >> "$F.view"
    column -t -s "$(printf '\t')" "$F.view"; rm -f "$F.view" ;;
  stalled)
    out=$(rows)
    agents=$(awk -F'\t' '{print $1}' "$HOME/BDSPLab/dispatch/claims.tsv" 2>/dev/null | sort -u | grep -c .)
    echo "agents running: $agents/3"
    [ -n "$out" ] && { echo "$out"; exit 1; }; echo "all lanes moving"; exit 0 ;;
  set)
    l=$2; f=$3; v=$4
    col=$(head -1 "$F" | tr '\t' '\n' | grep -n "^$f\$" | cut -d: -f1)
    [ -z "$col" ] && { echo "no field $f" >&2; exit 2; }
    awk -F'\t' -v OFS='\t' -v l="$l" -v c="$col" -v v="$v" -v t="$(date '+%Y-%m-%d %H:%M')" \
      '$1==l{$c=v; if (c==2) $4=t; hit=1} {print} END{if(!hit) exit 3}' "$F" > "$F.tmp" \
      && mv "$F.tmp" "$F" || { rm -f "$F.tmp"; echo "no lane $l" >&2; exit 3; }
    echo "$l.$f = $v" ;;
  *) echo "usage: lanes.sh show | stalled | set LANE FIELD VALUE" >&2; exit 2 ;;
esac
