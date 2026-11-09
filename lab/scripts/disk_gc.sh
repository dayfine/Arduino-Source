#!/bin/sh
# disk_gc.sh [--dry-run|--apply] : keep the lab's disk use bounded. Dry run by default (prints what it
# would delete); --apply deletes. The live copy is ~/BDSPLab/disk_gc.sh, run by launchd job
# com.dayfine.lab-disk-gc at 06:30 and hourly when free < 30 GB; this file is its source.
# Scope is our own lab output only:
#  1. SerialPrograms lab profiles (lab-A, lab-B): screenshots, CFR shots, debug dumps, local error reports
#     and rotated logs older than $AGE days (default 7); save screenshots are kept for 30 days.
#     Also: Lab/outbox and Lab/done keep the newest 100 files (a huge outbox hung the lab channel,
#     2026-11-02); battle move shots go after 2 days; route and save shots older than 2 days are
#     shrunk to 960 px JPEG (failure frames stay full size).
#  2. BDSPLab builds: keep the 3 newest per family (prune_builds.sh); stage-runs older than 14 days.
#  3. Agent git trees under ~/.claude/jobs/*/tmp, ~/Projects/wt and /private/tmp/claude-wt (and build folders of Claude agent worktrees
#     under $REPO/.claude/worktrees, idle 2 days): build folders untouched for 2 days are deleted;
#     a whole worktree is removed only if it is clean, its HEAD is on a remote branch, and git has not
#     touched it for 3 days. Never touches a build whose app is running.
#  4. Lab deployments (start_dev.py stages one per build in ~/Projects/dev-deployments/<slot>/): keep
#     the newest $DEPLOY_KEEP (default 5) per slot folder by mtime; never one a running SerialPrograms
#     process loads from; never a symlink. Build folders (~/Projects/dev-builds) are not touched.
# Trees listed in ~/BDSPLab/keep-worktrees.txt are never touched.
# Prints free space at the end and warns when it is below $MIN_FREE_GB (default 30).
APPLY=0
case "$1" in
  --apply) APPLY=1 ;;
  --dry-run|"") ;;
  *) echo "usage: disk_gc.sh [--dry-run|--apply]" >&2; exit 2 ;;
esac
AGE=${AGE:-7}; MIN_FREE_GB=${MIN_FREE_GB:-30}; DEPLOY_KEEP=${DEPLOY_KEEP:-5}
# Short on space: keep 3 days of lab output instead of 7.
[ $(( $(df -k "$HOME" | awk 'NR==2{print $4}') / 1048576 )) -lt $MIN_FREE_GB ] && AGE=3
S=$HOME/BDSPLab; P="$HOME/Library/Application Support/SerialPrograms/Profiles"
REPO=$HOME/Projects/Arduino-Source; DEPLOYS=$HOME/Projects/dev-deployments
# Agents' shared scratch for worktrees; overridable so the self-test never reaches the real one.
CLAUDE_WT=${CLAUDE_WT:-/private/tmp/claude-wt}
say(){ echo "disk_gc: $*"; }
del(){ if [ $APPLY = 1 ]; then rm -rf "$1"; fi; }
kb_before=$(df -k "$HOME" | awk 'NR==2{print $4}')

# 1. Profiles
for prof in lab-A lab-B; do
  d="$P/$prof"; [ -d "$d" ] || continue
  for sub in Screenshots CommandFiles DebugDumps ErrorReportsLocal Lab/runs; do
    [ -d "$d/$sub" ] || continue
    n=$(find "$d/$sub" -type f -mtime +$AGE ! -path '*/saves/*' | wc -l | tr -d ' ')
    [ "$n" -gt 0 ] && say "$prof/$sub: $n files older than ${AGE}d"
    [ $APPLY = 1 ] && find "$d/$sub" -type f -mtime +$AGE ! -path '*/saves/*' -delete
    [ $APPLY = 1 ] && find "$d/$sub" -type f -path '*/saves/*' -mtime +30 -delete
    [ $APPLY = 1 ] && find "$d/$sub" -mindepth 1 -type d -empty -delete
  done
  for box in Lab/outbox Lab/done; do
    [ -d "$d/$box" ] || continue
    n=$(ls -t "$d/$box" | tail -n +101 | wc -l | tr -d ' ')
    [ "$n" -gt 0 ] && say "$prof/$box: $n files beyond the newest 100"
    [ $APPLY = 1 ] && ls -t "$d/$box" | tail -n +101 | while read f; do rm -rf "$d/$box/$f"; done
  done
  if [ -d "$d/Screenshots/AutoStory/moves" ]; then
    n=$(find "$d/Screenshots/AutoStory/moves" -type f -mtime +2 | wc -l | tr -d ' ')
    [ "$n" -gt 0 ] && say "$prof: $n battle move shots older than 2d"
    [ $APPLY = 1 ] && find "$d/Screenshots/AutoStory/moves" -type f -mtime +2 -delete
  fi
  for sub in route saves; do
    [ -d "$d/Screenshots/AutoStory/$sub" ] || continue
    n=$(find "$d/Screenshots/AutoStory/$sub" -name '*.png' -mtime +2 | wc -l | tr -d ' ')
    [ "$n" -gt 0 ] && say "$prof: $n $sub shots to shrink to JPEG"
    [ $APPLY = 1 ] && find "$d/Screenshots/AutoStory/$sub" -name '*.png' -mtime +2 | while read f; do
      sips -Z 960 -s format jpeg -s formatOptions 80 "$f" --out "${f%.png}.jpg" >/dev/null 2>&1 && rm "$f"
    done
  done
  for f in "$d"/SerialPrograms-*.log; do
    [ -f "$f" ] || continue
    [ -n "$(find "$f" -mtime +$AGE)" ] && { say "$prof: old log $(basename "$f")"; del "$f"; }
  done
done

# 2. BDSPLab
if [ ! -f $S/prune_builds.sh ]; then :
elif [ $APPLY = 1 ]; then sh $S/prune_builds.sh 3; else say "builds: would run prune_builds.sh 3"; fi
find $S/stage-runs -mindepth 1 -maxdepth 1 -type d -mtime +14 2>/dev/null | while read r; do say "stage-run $(basename $r)"; del "$r"; done

# 3. Agent worktrees and clones: any git tree up to three levels under ~/.claude/jobs (job scratch and
#    agents' named folders like jobs/bagfix) and
#    /private/tmp/claude-wt (not only wt-*; 2026-11-02 they held 50 GB under other names).
SCRATCH="$HOME/.claude/jobs/*/tmp $HOME/.claude/jobs $HOME/Projects/wt $CLAUDE_WT"
trees=$(for r in $SCRATCH; do find $r -mindepth 1 -maxdepth 3 -name .git -prune 2>/dev/null; done | sed 's#/\.git$##')
for wt in $trees; do
  [ -d "$wt" ] && [ -e "$wt/.git" ] || continue
  # A repo nested in another tree (a lab tree's Packages) goes with its parent, never alone.
  git -C "$(dirname "$wt")" rev-parse --git-dir >/dev/null 2>&1 && continue
  name=$(basename "$wt")
  # Lab trees and anything a deployed build still loads from stay, build folder and all.
  grep -qxF "$wt" $S/keep-worktrees.txt 2>/dev/null && { say "$name: in keep-worktrees.txt, kept"; continue; }
  grep -rqs "$wt/" $S/builds/*/SerialPrograms.app/Contents/Info.plist 2>/dev/null && { say "$name: a deployed build uses it, kept"; continue; }
  gitdir=$(git -C "$wt" rev-parse --absolute-git-dir 2>/dev/null) || continue
  # Idle: HEAD hasn't moved and no source file changed for 3 days (git status rewrites the index,
  # so the index's time says nothing).
  touched_recent=$(find "$gitdir/logs/HEAD" -mmin -4320 2>/dev/null)
  [ -z "$touched_recent" ] && touched_recent=$(find "$wt" \( -name .git -o -name 'build*' \) -prune -o -type f -mmin -4320 -print -quit 2>/dev/null)
  if pgrep -f "$wt/" >/dev/null; then say "$name: a process runs from it, kept"; continue; fi
  if [ -z "$touched_recent" ] && [ -z "$(git -C "$wt" status --porcelain 2>/dev/null)" ] \
     && [ -n "$(git -C "$wt" branch -r --contains HEAD 2>/dev/null | head -1)" ]; then
    say "$name: clean, pushed, idle 3d: remove worktree ($(du -sh "$wt" | cut -f1))"
    if [ $APPLY = 1 ]; then
      if [ -f "$wt/.git" ]; then git -C "$wt" worktree remove --force "$wt"
      elif [ -z "$(git -C "$wt" stash list 2>/dev/null)" ]; then rm -rf "$wt"; fi
    fi
    continue
  fi
  for b in "$wt/SerialPrograms/build" "$wt/build-reorg" "$wt/build"; do
    [ -d "$b" ] || continue
    [ -n "$(find "$b" -maxdepth 1 -mmin -2880 2>/dev/null | head -1)" ] && continue
    say "$name: idle build folder $(basename "$b") ($(du -sh "$b" | cut -f1))"; del "$b"
  done
done
for b in $REPO/.claude/worktrees/*/SerialPrograms/build; do
  [ -d "$b" ] || continue
  pgrep -f "$(dirname "$(dirname "$b")")/" >/dev/null && continue
  [ -n "$(find "$b" -maxdepth 1 -mmin -2880 2>/dev/null | head -1)" ] && continue
  say "agent worktree $(basename "$(dirname "$(dirname "$b")")"): idle build folder ($(du -sh "$b" | cut -f1))"; del "$b"
done
[ $APPLY = 1 ] && [ -d $REPO ] && git -C $REPO worktree prune
# Plain (non-git) scratch folders idle 14 days, 7 when short on space; never one that holds a git tree.
SCR_AGE=14; [ $AGE = 3 ] && SCR_AGE=7
for r in $SCRATCH; do
  find $r -mindepth 1 -maxdepth 1 -type d 2>/dev/null | while read d; do
    [ -n "$(find "$d" -maxdepth 3 -name .git -print -quit 2>/dev/null)" ] && continue
    [ -f "$d/state.json" ] || [ "$(basename "$d")" = tmp ] && continue   # a job's own folder
    grep -qxF "$d" $S/keep-worktrees.txt 2>/dev/null && continue
    [ -n "$(find "$d" -mtime -$SCR_AGE -print -quit 2>/dev/null)" ] && continue
    pgrep -f "$d/" >/dev/null && continue
    say "scratch $(basename "$d"): idle ${SCR_AGE}d ($(du -sh "$d" | cut -f1))"; del "$d"
  done
done

# 4. Lab deployments: newest $DEPLOY_KEEP per slot folder stay. The keep rules are skip tests in the
#    loop, so a kept deployment is never even listed for deletion. mtime order: a deployment's folder
#    also changes when a run writes output into it, so "newest" means newest built or used.
# Full command lines (as `pgrep -fl SerialPrograms` shows on macOS; ps is the same on Linux).
running=$(ps -A -o command= 2>/dev/null | grep SerialPrograms)
for slot in "$DEPLOYS"/*; do
  [ -d "$slot" ] && [ ! -L "$slot" ] || continue
  n=0
  for name in $(ls -t "$slot"); do
    dep="$slot/$name"
    [ -d "$dep" ] && [ ! -L "$dep" ] || continue
    n=$((n + 1))
    [ $n -le "$DEPLOY_KEEP" ] && continue
    case "$running" in
      *"$dep/"*) say "deployment $(basename "$slot")/$name: a running SerialPrograms uses it, kept"; continue ;;
    esac
    say "deployment $(basename "$slot")/$name: beyond the newest $DEPLOY_KEEP ($(du -sh "$dep" | cut -f1))"; del "$dep"
  done
done

kb=$(df -k "$HOME" | awk 'NR==2{print $4}')
say "free $((kb/1048576)) GB (freed $(((kb-kb_before)/1048576)) GB)"
[ $((kb/1048576)) -lt $MIN_FREE_GB ] && say "WARNING: free space below ${MIN_FREE_GB} GB"
exit 0
