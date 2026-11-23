# Handoff: layered automation stack

Written 2026-11-22 for a fresh session on a different computer. It holds the current state
and what to do next. The design itself is in
[`Layered-Automation-Stack.md`](Layered-Automation-Stack.md) and is not repeated here;
section numbers below refer to it.

## State

- The design is complete as an interview outcome: 15 questions answered by the user and 8
  derived decisions (section 2). Nothing is built. No repo, no spike, no code.
- It is filed as fork draft PR dayfine/Arduino-Source#808 (base `agents`, branch
  `docs/layered-automation-stack`). The `Agents Check` job passed on the first commit. The
  PR is not merged.
- The user has not yet given a go-ahead to build anything. The last question put to them
  was whether to start the Week 0 link spike; it was not answered before the move.

## Do next

1. Read the design doc, then `AGENTS.md` and `pokemon_sv/PR_WORKFLOW.md` on `origin/agents`.
2. Get the user's decisions that block a first build (section 12):
   - the new repo's name and location;
   - which FRLG checkpoint is S0/S1 (it should use `from_save`, `route`, `heal_to_full`,
     `save`, the actions S4 needs first);
   - whether to merge #808.
3. Run the Week 0 spikes (section 10). What each one needs:

   | Spike | Needs | Can run on a computer without the lab |
   |---|---|---|
   | Link: `CoreLib` + image primitive sources in a Qt-free binary | an Arduino-Source checkout, CMake, OpenCV | yes |
   | Capture: OpenCV `VideoCapture` at 1080p | the lab capture card | no |
   | Latency: frame -> probe -> OCaml -> command, p99 under 33 ms | capture card, controller board, a console | no |

   The link spike is the only one that can start anywhere, and it needs no repo name: a
   scratch tree is enough. S0 (mGBA) also needs no console, but it needs mGBA 0.10.5 and
   the FireRed ROM, which the user loaded on the old computer only.
4. When the repo is created, set up D1 in the first commit: the architecture note, the
   lint on `dune` files, the dependency allowlist, the Windows CI build (section 6). Add
   the golden wire messages (D4) with the first protocol code.
5. Follow-ups noticed but not requested; ask before doing them:
   - the FRLG dex design's slices 2-3 (`autostory/frlg/design/AutoStory-FRLG-Dex-Collection.md`)
     assume C++ list inference and need redirecting to probes;
   - the pa-lab README plan should say that Slice 7b work on the old app stops (section 8).

## What the design doc does not say

Where the user overruled or corrected a recommendation. Keep these in mind before
reopening a decision.

- **Q5.** The recommendation was primitives plus a registry of existing C++ routines. The
  user chose primitives only. Their framing: the low-level Switch I/O is not worth
  reinventing; above it sits a feed-processing layer; above that a better programming
  layer for playing games, "not just pokemon games".
- **Q10.** The recommendation was S0, S1, S2, S3, S4. The user moved S4 (FRLG dex
  collection) directly after S1.
- **Q14.** The first recommendation was an MCP server inside the OCaml runner. It was
  withdrawn after reading pa-lab: it would rebuild a tested control plane, and its SQLite
  store needs C stubs, which breaks D1.
- **Q8** was answered "A for now". The user wants a clean way to grow it, which is why
  section 4 has the probe-provider concept.
- **Q2/Q3.** The user is interested in a multi-process pipeline (video feed processed by
  separate programs in other languages). The external worker is deferred, not rejected.
  The copy cost is negligible with a shared-memory frame ring; the real costs are
  ordering and latency, backpressure, process lifecycle, and a vision stack per worker.
- **Windows.** End users get a prebuilt native executable and never build OCaml. The cost
  of OCaml on Windows falls on development and CI (library gaps, C stubs), hence D1.
- **Terms.** "Word" was dropped for "action".

Not done, although the user raised it:

- No survey of other implementations of the I/O layer was made. The user said "maybe
  there are other impls for such a thing we can swap". The design only keeps backends
  behind one contract.
- No search for an existing RNG library was made. The user said "ideally find a new
  backend / library for the RNG, or rewrite it in Ocaml / python". D6 (rewrite in OCaml)
  is a default, not a discussed choice.

## Code pointers for the spikes

Read during the interview; none of this was built or run.

| Topic | Where |
|---|---|
| Qt-free core target | `SerialPrograms/cmake/CoreLib.cmake`, `CORE_LIBRARY_SOURCES` in `cmake/SourceFiles.cmake` |
| Existing headless entry point | `SerialPrograms/Source/CommandLine/CommandLine_Main.cpp` |
| Controller interface to mirror | `NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h`, `Controllers/Controller.h` |
| Qt touchpoints inside the image primitives | `CommonFramework/ImageTypes/ImageRGB32_Qt.{h,cpp}`, `CommonTools/OCR/OCR_StringNormalization.cpp` |
| Video backends (all Qt Multimedia, not reused) | `CommonFramework/VideoPipeline/Backends/` |
| Frame type and timestamps | `CommonFramework/VideoPipeline/VideoFeed.h` (`VideoSnapshot`) |
| Default inference period (50 ms video) | `CommonTools/Async/InferenceSession.h` |
| Generic detectors to re-express as probes | `CommonTools/VisualDetectors/` |
| Observation shape to copy for the per-frame record | `CommonTools/ScreenMachine/ScreenObservation.h` on fork branch `autostory/engine` |
| Sockets and JSON in the core | `Common/Cpp/Sockets/`, `Common/Cpp/Json/` |
| BDSP RNG reference (oracle for S2) | `Pokemon/Pokemon_Xorshift128.h`, `Pokemon/Pokemon_BdspRng.h`, `PokemonBDSP/Programs/RngManipulation/`, `PokemonBDSP/Inference/Rng/` |
| Model users (S3) | `ML/Inference/ML_YOLONavigation.h`, SV AutoStory segments 35-39, `PokemonLZA_ShinyHunt_HyperspaceLegendary.cpp` |
| pa-lab | `lab/server/` on `agents` (`GUIDE.md`, `README.md`, `contracts/v1/`); design in `docs/design/Agent-Integration.md` on `lab/tooling` |
| mGBA lane | `docs/design/Lab-mGBA-Console.md`, `lab/mgba-*` branches (not examined for reuse) |

## Moving to another computer

These live outside the repository and do not travel with a clone.

| Item | On the old computer | Note |
|---|---|---|
| Global agent rules | `~/.claude/CLAUDE.md` | The PR rules are also on `origin/agents`; the worktree and disk rules are only here |
| Upstream guard | `~/.local/bin/gh` (a wrapper, not in any repo) | Until it is installed, the rule holds by discipline: never open, reopen or mark ready a PR on `PokemonAutomation/Arduino-Source`; create fork PRs with `--repo dayfine/Arduino-Source --draft` |
| Agent memory | `~/.claude/projects/<slug>/memory/` (160 entries) | The slug is derived from the checkout's absolute path, so the folder name must match the new path |
| Skills | `~/.claude/skills/` | `grill-me`, `handoff`, `prototype`, `tdd`, the lab skills |
| Clones | `Arduino-Source` (remotes `origin` = fork, `upstream`), `PokemonSimulation`, `pokemonautomation.github.io`, `CommandLineTests` | One primary clone per repo; side tasks use `git worktree add ~/Projects/wt/<repo>-<task>` |
| Lab | console B, capture card, `~/BDSPLab`, the pa-lab server and its key file, mGBA and the ROM | Console and capture spikes stay with this hardware |
| Scheduled jobs | launchd: daily fork sync, lab disk GC | Per machine |

Uncommitted work left on the old computer, not part of this design: the main checkout is on
`upstream/frlg-free-party-slots` with staged changes to three FRLG files and is 6 commits
behind its remote. Other worktrees were not audited.

The pa-lab MCP server failed to connect during the design session, so its plan was read
from git, not from a running server.

## Suggested skills

- `grill-me` or `grill-with-docs`: to reopen a decision or settle the open items in section 12.
- `prototype`: for the Week 0 spikes, which are throwaway.
- `tdd`: for the OCaml kernel, readers and the wire protocol once the repo exists.
- `to-issues`: to split slices S0-S4 into issues after the go-ahead.
- `diagnose`: when a spike or a gate fails.
- `console-iteration`: for S1 on console B.
- `dispatch`: only if the work is split across agents.
