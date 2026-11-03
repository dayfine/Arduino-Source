---
name: map-route-verify
description: Build or fix a navigation tile grid from a Bulbapedia map (and pret collision data), plan a route on it, and verify grid and route on the lab console before coding a story checkpoint. Use when adding a new map to AutoStory navigation, when a walk ended somewhere the plan didn't expect, or when asked to check, draw or fix a map grid or route.
---

# Map, route, walk

The process lives in two places; read both before starting:

1. **`devtools/maps/README.md`** (fork PR #83, branch `devtools/map-grid`;
   `git show origin/devtools/map-grid:devtools/maps/README.md`): the tools (`levels.py`,
   `python3 -m devtools.maps fetch|overlay`) and the process, "Map, route,
   walk": which layer failed, how to measure a map with CFR `walk`, how to
   plan routes anchor to anchor.
2. **`autostory/LAB_NAVIGATION.md`** on the `agents` notes branch
   (`git show origin/agents:autostory/LAB_NAVIGATION.md`): our lab setup (console B, the `cfrwalk1` build, reloading to an
   anchor) and the traps that cost runs.

In short:
- A failing route: name the layer first (map / route / walk) and fix that layer.
- Map facts come only from console measurements (`walk` legs, sign reads),
  never from screenshots or timed holds; they go in the data file's header.
- Routes are planned and tested offline, every leg ending on a wall, a sign or
  an NPC; draw them with `overlay` and show the user before the first run.
- Console rules: the `bdsp-autostory-test` skill (guard, console B only, stop
  on failure, never a new game without OK).
