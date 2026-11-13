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
#     the newest $DEPLOY_KEEP (default 3) per slot folder by mtime; never one a running SerialPrograms
#     process loads from; never a symlink. Build folders (~/Projects/dev-builds) are not touched.
#  5. Media in every profile's Screenshots, ErrorReportsLocal, DebugDumps, CommandFiles/outbox: PNG older
#     than 6 h -> same-name JPEG q80; mp4/mkv older than 1 day re-encoded (ffmpeg crf 30, <=720p, once);
#     media older than 14 days deleted except under ErrorReportsLocal (and */saves/*, 30 days as above).
#  6. ccache (~/Library/Caches/pokemonautomation-ccache): max size 8G, cleanup each run.
#  7. ~/Projects/dev-builds: a slot build folder not built in 3 days goes (slot-A, slot-B, slot-B-frlg stay).
# Trees listed in ~/BDSPLab/keep-worktrees.txt, lab slot trees/builds named in
# ~/.config/pokemonautomation/dev-labs*.json, locked worktrees and paths a process runs in are never touched.
# Prints free space at the end and warns when it is below $MIN_FREE_GB (default 30).
APPLY=0
case "$1" in
  --apply) APPLY=1 ;;
  --dry-run|"") ;;
  *) echo "usage: disk_gc.sh [--dry-run|--apply]" >&2; exit 2 ;;
esac
AGE=${AGE:-7}; MIN_FREE_GB=${MIN_FREE_GB:-30}; DEPLOY_KEEP=${DEPLOY_KEEP:-3}
# Short on space: keep 3 days of lab output instead of 7.
# Worktrees: a clean, pushed tree is cheap to recreate, so it goes after 12 h idle (6 h when
# short on space); in-tree build folders after 6 h. User 2026-11-11 (twice): 3 d, then 1 d, too slow.
TREE_IDLE_MIN=720; BUILD_IDLE_MIN=360
[ $(( $(df -k "$HOME" | awk 'NR==2{print $4}') / 1048576 )) -lt $MIN_FREE_GB ] && { AGE=3; TREE_IDLE_MIN=360; }
S=$HOME/BDSPLab; P="$HOME/Library/Application Support/SerialPrograms/Profiles"
REPO=$HOME/Projects/Arduino-Source; DEPLOYS=$HOME/Projects/dev-deployments
# Agents' shared scratch for worktrees; overridable so the self-test never reaches the real one.
CLAUDE_WT=${CLAUDE_WT:-/private/tmp/claude-wt}
say(){ echo "disk_gc: $*"; }
del(){ if [ $APPLY = 1 ]; then rm -rf "${1:?}"; fi; }
kb_before=$(df -k "$HOME" | awk 'NR==2{print $4}')
# Paths in use: every command line plus the working folder of ninja/cmake/SerialPrograms processes
# (`ninja` runs with no path in its arguments; lsof +D over 600 trees is far too slow).
in_use=$( { ps -A -o command= 2>/dev/null
  for pid in $(pgrep -x ninja; pgrep cmake; pgrep -f SerialPrograms); do
    lsof -a -p "$pid" -d cwd -Fn 2>/dev/null | sed -n 's/^n//p'; done; } | sed 's#$#/#')
busy(){ printf '%s\n' "$in_use" | grep -qF "$1/"; }
# Lab slot worktrees and build folders (start_dev.py configs); values may start with ~.
lab_paths=$(sed -En 's/.*"(worktree|build_dir|stage_root)": *"([^"]*)".*/\2/p' $HOME/.config/pokemonautomation/dev-labs*.json 2>/dev/null \
  | sed "s#^~#$HOME#" | sort -u)
lab_path(){ printf '%s\n' "$lab_paths" | grep -qxF "$1"; }

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
    [ $APPLY = 1 ] && ls -t "$d/$box" | tail -n +101 | while read f; do rm -rf "$d/$box/${f:?}"; done
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
  lab_path "$wt" && { say "$name: lab slot worktree, kept"; continue; }
  grep -rqs "$wt/" $S/builds/*/SerialPrograms.app/Contents/Info.plist 2>/dev/null && { say "$name: a deployed build uses it, kept"; continue; }
  gitdir=$(git -C "$wt" rev-parse --absolute-git-dir 2>/dev/null) || continue
  # Idle: HEAD hasn't moved and no source file changed for TREE_IDLE_MIN (git status rewrites the
  # index, so the index's time says nothing).
  touched_recent=$(find "$gitdir/logs/HEAD" -mmin -$TREE_IDLE_MIN 2>/dev/null)
  [ -z "$touched_recent" ] && touched_recent=$(find "$wt" \( -name .git -o -name 'build*' \) -prune -o -type f -mmin -$TREE_IDLE_MIN -print -quit 2>/dev/null)
  if busy "$wt"; then say "$name: a process runs in it, kept"; continue; fi
  # Removable: no tracked changes (untracked build output and logs don't count, untracked
  # .cpp/.h/.py/.md do), every commit of HEAD on some remote branch, not locked.
  common=$(git -C "$wt" rev-parse --path-format=absolute --git-common-dir 2>/dev/null)
  locked=; [ -f "$gitdir/locked" ] && locked=1
  if [ -z "$touched_recent" ] && [ -z "$locked" ] \
     && [ -z "$(git -C "$wt" status --porcelain --untracked-files=no 2>/dev/null)" ] \
     && [ -z "$(git -C "$wt" ls-files --others --exclude-standard 2>/dev/null | grep -E '\.(cpp|h|py|md)$' | head -1)" ] \
     && git -C "$wt" rev-parse -q --verify HEAD >/dev/null \
     && [ -z "$(git -C "$wt" rev-list -1 HEAD --not --remotes 2>/dev/null)" ]; then
    say "$name: clean, pushed, idle ${TREE_IDLE_MIN}m: remove worktree ($(du -sh "$wt" | cut -f1))"
    if [ $APPLY = 1 ]; then
      if [ -f "$wt/.git" ]; then
        owner=$(dirname "$common")
        git -C "${owner:?}" worktree remove --force "${wt:?}" && git -C "$owner" worktree prune
      elif [ -z "$(git -C "$wt" stash list 2>/dev/null)" ] && [ -z "$(git -C "$wt" rev-list -1 --branches --not --remotes)" ]; then
        rm -rf "${wt:?}"
      fi
    fi
    continue
  fi
  for b in "$wt"/SerialPrograms/build* "$wt"/build*; do
    [ -d "$b" ] && [ ! -L "$b" ] || continue
    lab_path "$b" && continue
    busy "$b" && { say "$name: build folder $(basename "$b") in use, kept"; continue; }
    [ -n "$(find "$b" -maxdepth 1 -mmin -$BUILD_IDLE_MIN 2>/dev/null | head -1)" ] && continue
    say "$name: idle build folder $(basename "$b") ($(du -sh "$b" | cut -f1))"; del "${b:?}"
  done
done
for b in $REPO/.claude/worktrees/*/SerialPrograms/build; do
  [ -d "$b" ] || continue
  busy "$(dirname "$(dirname "$b")")" && continue
  [ -n "$(find "$b" -maxdepth 1 -mmin -$BUILD_IDLE_MIN 2>/dev/null | head -1)" ] && continue
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

# 5. Media in every profile. Never the live log: only image/video files are touched, and only old ones.
FFMPEG=$(command -v ffmpeg || ls /opt/homebrew/bin/ffmpeg 2>/dev/null)
FFPROBE=$(command -v ffprobe || ls /opt/homebrew/bin/ffprobe 2>/dev/null)
for d in "$P"/*/; do
  d=${d%/}; prof=$(basename "$d")
  for sub in Screenshots ErrorReportsLocal DebugDumps CommandFiles/outbox; do
    [ -d "$d/$sub" ] || continue
    n=$(find "$d/$sub" -type f -iname '*.png' -mmin +360 | wc -l | tr -d ' ')
    [ "$n" -gt 0 ] && say "$prof/$sub: $n PNGs older than 6h to JPEG"
    [ $APPLY = 1 ] && find "$d/$sub" -type f -iname '*.png' -mmin +360 | while IFS= read -r f; do
      j="${f%.*}.jpg"; [ -e "$j" ] && continue
      sips -s format jpeg -s formatOptions 80 "$f" --out "$j" >/dev/null 2>&1
      if [ -s "$j" ]; then touch -r "$f" "$j"; rm -f "${f:?}"; else rm -f "$j"; fi
    done
    if [ -n "$FFMPEG" ] && [ -n "$FFPROBE" ]; then
      find "$d/$sub" -type f \( -iname '*.mp4' -o -iname '*.mkv' \) -mtime +1 | while IFS= read -r f; do
        "$FFPROBE" -v error -show_entries format_tags=comment -of csv=p=0 "$f" 2>/dev/null | grep -q disk_gc && continue
        say "$prof/$sub: re-encode $(basename "$f") ($(du -h "$f" | cut -f1))"
        [ $APPLY = 1 ] || continue
        t="${f%.*}.gc-tmp.${f##*.}"
        if "$FFMPEG" -nostdin -v error -y -i "$f" -vf "scale=-2:'min(720,ih)'" -c:v libx264 -crf 30 -preset veryfast \
             -c:a aac -b:a 96k -metadata comment=disk_gc "$t" && [ -s "$t" ] \
           && [ "$(stat -f %z "$t")" -lt "$(stat -f %z "$f")" ]; then
          touch -r "$f" "$t"; mv -f "$t" "$f"
        else rm -f "${t:?}"; fi
      done
    fi
    [ "$sub" = ErrorReportsLocal ] && continue
    n=$(find "$d/$sub" -type f \( -iname '*.png' -o -iname '*.jpg' -o -iname '*.mp4' -o -iname '*.mkv' \) -mtime +14 ! -path '*/saves/*' | wc -l | tr -d ' ')
    [ "$n" -gt 0 ] && say "$prof/$sub: $n media files older than 14d"
    [ $APPLY = 1 ] && find "$d/$sub" -type f \( -iname '*.png' -o -iname '*.jpg' -o -iname '*.mp4' -o -iname '*.mkv' \) -mtime +14 ! -path '*/saves/*' -delete
  done
done

# 6. ccache: 8G cap (it sat at its 15 GB cap), then trim.
CC_DIR=$HOME/Library/Caches/pokemonautomation-ccache
if [ -d "$CC_DIR" ] && command -v ccache >/dev/null; then
  if [ $APPLY = 1 ]; then CCACHE_DIR=$CC_DIR ccache -M 8G >/dev/null && CCACHE_DIR=$CC_DIR ccache -c >/dev/null; fi
  say "ccache: $(du -sh "$CC_DIR" | cut -f1) (cap 8G)"
fi

# 7. Slot build folders not built for 3 days. The active slots stay whatever their age.
for b in $HOME/Projects/dev-builds/*/; do
  b=${b%/}; [ -f "$b/CMakeCache.txt" ] && [ ! -L "$b" ] || continue
  case "$(basename "$b")" in slot-A|slot-B|slot-B-frlg) continue ;; esac
  busy "$b" && { say "dev-build $(basename "$b"): in use, kept"; continue; }
  [ -n "$(find "$b" -maxdepth 1 \( -name .ninja_log -o -name build.ninja -o -name CMakeCache.txt \) -mtime -3 | head -1)" ] && continue
  say "dev-build $(basename "$b"): not built for 3d ($(du -sh "$b" | cut -f1))"; del "${b:?}"
done

kb=$(df -k "$HOME" | awk 'NR==2{print $4}')
say "free $((kb/1048576)) GB (freed $(((kb-kb_before)/1048576)) GB)"
[ $((kb/1048576)) -lt $MIN_FREE_GB ] && say "WARNING: free space below ${MIN_FREE_GB} GB"
exit 0
