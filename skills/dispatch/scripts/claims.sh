#!/bin/sh
# claims.sh claim AGENT BRANCH... | release AGENT | list | check BRANCH...
# Branch ownership for dispatched subagents: one agent per branch at a time.
# File: ~/BDSPLab/dispatch/claims.tsv (agent<TAB>branch<TAB>since). Exit 1 on a conflict.
F="$HOME/BDSPLab/dispatch/claims.tsv"; touch "$F"
cmd=$1; shift
case $cmd in
  claim)
    a=$1; shift; bad=0
    for b in "$@"; do
      o=$(awk -F'\t' -v b="$b" '$2==b{print $1}' "$F")
      if [ -n "$o" ] && [ "$o" != "$a" ]; then echo "CONFLICT $b held by $o"; bad=1; fi
    done
    [ $bad = 1 ] && exit 1
    for b in "$@"; do
      awk -F'\t' -v b="$b" '$2!=b' "$F" > "$F.tmp" && mv "$F.tmp" "$F"
      printf '%s\t%s\t%s\n' "$a" "$b" "$(date '+%Y-%m-%d %H:%M')" >> "$F"
    done; echo "claimed: $*" ;;
  release) awk -F'\t' -v a="$1" '$1!=a' "$F" > "$F.tmp" && mv "$F.tmp" "$F"; echo "released: $1" ;;
  check)
    bad=0; for b in "$@"; do o=$(awk -F'\t' -v b="$b" '$2==b{print $1}' "$F"); [ -n "$o" ] && { echo "HELD $b by $o"; bad=1; }; done; exit $bad ;;
  list) column -t -s "$(printf '\t')" "$F" 2>/dev/null || cat "$F" ;;
  *) echo "usage: claims.sh claim AGENT BRANCH... | release AGENT | check BRANCH... | list" >&2; exit 2 ;;
esac
