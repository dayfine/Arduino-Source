#!/bin/sh
# disk_gc_test.sh: the deployment rule of disk_gc.sh on a fake HOME. One slot holds 8 deployments
# (d1 oldest .. d8 newest), a symlink and a running "SerialPrograms" from d1. Checks that the dry run
# deletes nothing, and that --apply keeps d4..d8 (newest 5), d1 (running) and the symlink, and
# deletes d2 and d3 only. A symlinked slot folder is skipped.
# Section 8 on fake GP_RUNS/CLAUDE_TMP: a KEEP run is never listed, a run untouched 14 days is emptied
# except its *.ss/*.sav*, a recent run keeps its files but gzips wire logs older than 1 day, and a
# scratchpad loses files older than 3 days (save states kept) and its empty folders.
# Section 2b: loose files at the top of ~/Projects/wt older than 1 day: notes/scripts archived, the
# rest deleted, newer ones kept. Section 5: a PLZA-capture PNG younger than 3 days is never converted
# to JPEG; an older one and other PNGs older than 6 h are. Exit 0 when it holds.
here=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d) || exit 2
fail=0
check(){ if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }
slot=$T/home/Projects/dev-deployments/slot-X
mkdir -p "$slot" "$T/elsewhere/e1" "$T/elsewhere/e2" "$T/wt"
i=1
while [ $i -le 8 ]; do
  mkdir -p "$slot/d$i/SerialPrograms.app/Contents/MacOS"
  touch -t "2026110${i}0000" "$slot/d$i"
  i=$((i + 1))
done
ln -s "$T/elsewhere/e1" "$slot/link"
ln -s "$T/elsewhere" "$T/home/Projects/dev-deployments/slot-L"
# The fake app: a process whose command line names d1, as `pgrep -fl SerialPrograms` would list it.
app=$slot/d1/SerialPrograms.app/Contents/MacOS/SerialPrograms
printf '#!/bin/sh\nsleep 60\n' > "$app"; chmod +x "$app"
"$app" & pid=$!
# Section 8 fixtures. ago N = a touch -t stamp N days back.
ago(){ date -v-"$1"d +%Y%m%d%H%M; }
gp=$T/gp; sp=$T/tmp/claude-501/proj/sess/scratchpad
mkdir -p "$gp/keep-run" "$gp/old-run/sub" "$gp/recent-run" "$gp/fresh-run" "$sp/sub"
for f in keep-run/KEEP keep-run/wire.jsonl keep-run/a.ss old-run/wire.jsonl old-run/notes.txt \
    old-run/sub/x.log old-run/state.ss old-run/game.sav old-run/game.sav.bak; do
  echo x > "$gp/$f"; touch -t "$(ago 20)" "$gp/$f"
done
echo x > "$gp/recent-run/old.jsonl"; touch -t "$(ago 5)" "$gp/recent-run/old.jsonl"
echo x > "$gp/recent-run/new.jsonl"; echo x > "$gp/fresh-run/new.jsonl"
for f in old.txt old.ss sub/old2.txt; do echo x > "$sp/$f"; touch -t "$(ago 5)" "$sp/$f"; done
echo x > "$sp/new.txt"
# Section 2b and 5 fixtures (all under the fake HOME).
wt=$T/home/Projects/wt; shots="$T/home/Library/Application Support/SerialPrograms/Profiles/test-P/Screenshots"
mkdir -p "$wt" "$shots/PLZA-capture"
for f in old.log old.md; do echo x > "$wt/$f"; touch -t "$(ago 2)" "$wt/$f"; done
echo x > "$wt/new.log"
# A 1x1 PNG so sips can convert it.
python3 -I -c 'import sys,zlib,struct
c=lambda t,d: struct.pack(">I",len(d))+t+d+struct.pack(">I",zlib.crc32(t+d))
sys.stdout.buffer.write(b"\x89PNG\r\n\x1a\n"+c(b"IHDR",struct.pack(">IIBBBBB",1,1,8,2,0,0,0))+c(b"IDAT",zlib.compress(b"\0\0\0\0"))+c(b"IEND",b""))' > "$T/px.png"
for f in PLZA-capture/cap1d PLZA-capture/cap4d other; do cp "$T/px.png" "$shots/$f.png"; done
touch -t "$(ago 1)" "$shots/PLZA-capture/cap1d.png" "$shots/other.png"; touch -t "$(ago 4)" "$shots/PLZA-capture/cap4d.png"
run(){ HOME=$T/home CLAUDE_WT=$T/wt GP_RUNS=$gp CLAUDE_TMP=$T/tmp DEPLOY_KEEP=5 sh "$here/disk_gc.sh" "$@" > "$T/out" 2>&1; }

run --dry-run; rc=$?
check "dry run exits 0" '[ $rc = 0 ]'
check "dry run lists d2 and d3" 'grep -q "slot-X/d2: beyond" "$T/out" && grep -q "slot-X/d3: beyond" "$T/out"'
check "dry run reports running d1 kept" 'grep -q "slot-X/d1: a running SerialPrograms uses it, kept" "$T/out"'
check "dry run deletes nothing" '[ -d "$slot/d2" ] && [ -d "$slot/d3" ]'
check "newest 5 never listed" '! grep -Eq "slot-X/d[4-8]" "$T/out"'
check "symlinks never listed" '! grep -Eq "slot-X/link|slot-L/" "$T/out"'
check "dry run lists old run, recent run logs and scratchpad" 'grep -q "gamepilot run old-run: untouched" "$T/out" && grep -q "gamepilot run recent-run: 1 wire logs" "$T/out" && grep -q "scratchpad sess: 2 files" "$T/out"'
check "dry run touches no run or scratchpad" '[ -f "$gp/old-run/notes.txt" ] && [ -f "$gp/recent-run/old.jsonl" ] && [ -f "$sp/old.txt" ]'
check "KEEP run never listed" '! grep -q "keep-run" "$T/out"'
check "fresh run never listed" '! grep -q "fresh-run" "$T/out"'
check "dry run lists old wt loose files only" 'grep -q "wt loose file old.md: archive" "$T/out" && grep -q "wt loose file old.log" "$T/out" && ! grep -q "new.log" "$T/out"'
check "dry run moves no wt loose file" '[ -f "$wt/old.md" ] && [ -f "$wt/old.log" ]'
check "PLZA-capture PNG under 3 days not counted" 'grep -q "test-P/Screenshots: 2 PNGs older than 6h" "$T/out"'

run --apply
check "apply deletes d2 and d3" '[ ! -e "$slot/d2" ] && [ ! -e "$slot/d3" ]'
check "apply keeps d1 and d4..d8" 'for k in 1 4 5 6 7 8; do [ -d "$slot/d$k" ] || exit 1; done'
check "apply keeps symlinks and their targets" '[ -L "$slot/link" ] && [ -d "$T/elsewhere/e1" ] && [ -d "$T/elsewhere/e2" ]'
check "apply leaves KEEP run whole" '[ -f "$gp/keep-run/KEEP" ] && [ -f "$gp/keep-run/wire.jsonl" ] && [ -f "$gp/keep-run/a.ss" ]'
check "apply empties old run but keeps save states" '[ -f "$gp/old-run/state.ss" ] && [ -f "$gp/old-run/game.sav" ] && [ -f "$gp/old-run/game.sav.bak" ] && [ ! -e "$gp/old-run/wire.jsonl" ] && [ ! -e "$gp/old-run/notes.txt" ] && [ ! -e "$gp/old-run/sub" ]'
check "apply gzips old wire log in recent run" '[ -f "$gp/recent-run/old.jsonl.gz" ] && [ ! -e "$gp/recent-run/old.jsonl" ]'
check "apply keeps recent run files" '[ -f "$gp/recent-run/new.jsonl" ] && [ -f "$gp/fresh-run/new.jsonl" ]'
check "apply cleans scratchpad, keeps save state and new file" '[ ! -e "$sp/old.txt" ] && [ ! -e "$sp/sub" ] && [ -f "$sp/old.ss" ] && [ -f "$sp/new.txt" ]'
check "apply archives old.md, deletes old.log, keeps new.log" '[ -f "$T/home/BDSPLab/archive/wt-loose/old.md" ] && [ ! -e "$wt/old.md" ] && [ ! -e "$wt/old.log" ] && [ -f "$wt/new.log" ]'
check "apply keeps PLZA-capture PNG under 3 days" '[ -f "$shots/PLZA-capture/cap1d.png" ] && [ ! -e "$shots/PLZA-capture/cap1d.jpg" ]'
check "apply converts older PLZA-capture and other PNGs" '[ -s "$shots/PLZA-capture/cap4d.jpg" ] && [ ! -e "$shots/PLZA-capture/cap4d.png" ] && [ -s "$shots/other.jpg" ] && [ ! -e "$shots/other.png" ]'

kill $pid 2>/dev/null; wait $pid 2>/dev/null
HOME=$T/home CLAUDE_WT=$T/wt GP_RUNS=$gp CLAUDE_TMP=$T/tmp sh "$here/disk_gc.sh" --bogus >/dev/null 2>&1; rc=$?
check "unknown option exits 2" '[ $rc = 2 ]'
[ $fail = 0 ] || cat "$T/out"
rm -rf "$T"
exit $fail
