#!/bin/sh
# checkin.sh : one compact status block for the overnight check-in (read this, not the logs).
P="$HOME/Library/Application Support/SerialPrograms/Profiles/lab-B"; L="$P/SerialPrograms.log"
echo "time $(date +%H:%M)"
h=$(sed -n 's/^owner=//p' "$P/Lab/hold" 2>/dev/null | tr -d '\r'); [ -n "$h" ] && echo "HOLD: console held by $h since $(sed -n 's/^acquired_at=//p' "$P/Lab/hold" | tr -d '\r') ($(sed -n 's/^reason=//p' "$P/Lab/hold" | tr -d '\r')): hands off; hold.sh off releases" || echo "hold: free"
pid=$(pgrep -f "SerialPrograms --profile lab-B" | head -1); echo "lab app: ${pid:-NOT RUNNING} $(ps -o command= -p ${pid:-1} 2>/dev/null | sed -n 's|.*deployments/\([^/]*/[^/]*\)/.*|\1|p')"
tail -c 2000000 "$L" | grep -a "Program State:" | tail -1 | cut -c12-19,40-
tail -c 2000000 "$L" | grep -aE "\[Checkpoint\] [0-9.a-z]+: (passed|failed)|Starting Program:" | tail -2 | cut -c12-19,40-220
last=$(tail -c 2000000 "$L" | grep -a "issue_" | grep -av nop | tail -1 | cut -c12-19); echo "last press $last"
f=$(ls -t "$P"/Screenshots/AutoStory/failures/* 2>/dev/null | head -1); echo "newest failure frame: $(basename "$f")"
for n in ${PRS:-}; do ~/.local/bin/gh pr checks $n -R dayfine/Arduino-Source 2>/dev/null | awk -v n=$n '{s[$2]++} END{printf "PR %s:",n; for(k in s) printf " %s=%d",k,s[k]; print ""}'; done
[ "${NOSHOT:-}" ] || sh ~/BDSPLab/shotcheck.sh 2>&1 | head -1
sh ~/BDSPLab/spend.sh
df -g ~ | awk 'NR==2{print "disk free " $4 " GB"}'
