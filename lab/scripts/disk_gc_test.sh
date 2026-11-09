#!/bin/sh
# disk_gc_test.sh: the deployment rule of disk_gc.sh on a fake HOME. One slot holds 8 deployments
# (d1 oldest .. d8 newest), a symlink and a running "SerialPrograms" from d1. Checks that the dry run
# deletes nothing, and that --apply keeps d4..d8 (newest 5), d1 (running) and the symlink, and
# deletes d2 and d3 only. A symlinked slot folder is skipped. Exit 0 when it holds.
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
run(){ HOME=$T/home CLAUDE_WT=$T/wt DEPLOY_KEEP=5 sh "$here/disk_gc.sh" "$@" > "$T/out" 2>&1; }

run --dry-run; rc=$?
check "dry run exits 0" '[ $rc = 0 ]'
check "dry run lists d2 and d3" 'grep -q "slot-X/d2: beyond" "$T/out" && grep -q "slot-X/d3: beyond" "$T/out"'
check "dry run reports running d1 kept" 'grep -q "slot-X/d1: a running SerialPrograms uses it, kept" "$T/out"'
check "dry run deletes nothing" '[ -d "$slot/d2" ] && [ -d "$slot/d3" ]'
check "newest 5 never listed" '! grep -Eq "slot-X/d[4-8]" "$T/out"'
check "symlinks never listed" '! grep -Eq "slot-X/link|slot-L/" "$T/out"'

run --apply
check "apply deletes d2 and d3" '[ ! -e "$slot/d2" ] && [ ! -e "$slot/d3" ]'
check "apply keeps d1 and d4..d8" 'for k in 1 4 5 6 7 8; do [ -d "$slot/d$k" ] || exit 1; done'
check "apply keeps symlinks and their targets" '[ -L "$slot/link" ] && [ -d "$T/elsewhere/e1" ] && [ -d "$T/elsewhere/e2" ]'

kill $pid 2>/dev/null; wait $pid 2>/dev/null
HOME=$T/home CLAUDE_WT=$T/wt sh "$here/disk_gc.sh" --bogus >/dev/null 2>&1; rc=$?
check "unknown option exits 2" '[ $rc = 2 ]'
[ $fail = 0 ] || cat "$T/out"
rm -rf "$T"
exit $fail
