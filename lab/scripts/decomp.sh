#!/bin/sh
# decomp.sh ARGS... : run `python3 -m devtools.maps` (fetch, overlay, decomp lookups) in isolated
# mode from a worktree that has devtools/maps (MAPGRID_WT, default ~/Projects/Arduino-Source-frlglab).
WT=${MAPGRID_WT:-"$HOME/Projects/Arduino-Source-frlglab"}
[ -d "$WT/devtools/maps" ] || { echo "decomp.sh: no devtools/maps in MAPGRID_WT=$WT" >&2; exit 2; }
exec python3 -I -c 'import sys, runpy; sys.path.insert(0, sys.argv[1]); sys.argv = ["devtools.maps"] + sys.argv[2:]; runpy.run_module("devtools.maps", run_name="__main__", alter_sys=False)' "$WT" "$@"
