# Layered automation stack: I/O daemon, probe layer, OCaml programs

Status: **design only, nothing built**. Outcome of two design interviews on 2026-11-22; the
second one (section 13) revised the first and wins where they differ.
Every decision below was chosen by the user unless marked *default* (proposed, not
discussed, open to objection). Items marked *unverified* are assumptions the Week 0 spikes
test.

## 1. Summary

Keep the hard-won low-level parts of Arduino-Source (controller protocol, scheduler, image
primitives), put them in a small Qt-free daemon with a JSON-RPC API, and write all game
logic in a separate OCaml/dune project. The daemon knows no game. Programs are typed data
plus small readers and actions, run identically against the console, a replay of recorded
measurements, or an in-process simulation.

```
Layer 3  Programs (OCaml)      stories, checkpoints, actions, readers, planner, sim
            │  JSON-RPC 2.0 over localhost TCP (facts up, command batches down)
Layer 2  Processing (C++)      probes: pixels -> numbers, keyed by frame seq
Layer 1  I/O (C++)             frame source + input sink + one clock
            │
         Switch via capture card and PABotBase2  |  mGBA  |  recorded frames
```

Not a rewrite of the existing 250 programs. The C++ AutoStory chains continue in parallel
until the new stack passes a measured gate (section 9).

## 2. Decisions

| # | Question | Decision |
|---|---|---|
| Q1 | Audience | Nothing needs to go upstream; keep the overlay on Arduino-Source thin so upstream changes are cheap to pick up (revised, section 13) |
| D1 | OCaml purity | OCaml side is pure logic plus a network client: no C stubs, dependency allowlist, Windows CI build as the real gate |
| Q2 | Where vision runs | In the daemon; OCaml never handles pixels in the hot loop |
| Q3 | Detector backends | One probe interface; in-daemon primitives now, external worker process designed for but deferred |
| Q4 | Time logic (watchers, settle rule, screen state machine) | OCaml, over a per-frame stream |
| D2 | Command model | Mirror the existing controller interface as timed batches (from code) |
| Q5 | Existing C++ above primitives | None exposed: no program runner, no routine registry |
| D3 | Game knowledge | No game-specific C++ in layers 1-2, including existing named detectors |
| Q6 | Where layer 2 lives | New repo, one process, Qt-free from day one (own capture) |
| Q7 | Wire | JSON-RPC 2.0 over localhost TCP; pixels out of band |
| D4 | Schema truth | OCaml types; checked-in golden messages that the C++ side must parse (*default*) |
| Q8 | What the daemon returns | Measurements (raw numbers), not verdicts; "for now" |
| D5 | Timing | Per-probe `latest` or `every` delivery; command batches can be anchored to the frame clock |
| D6 | RNG math | Rewritten in OCaml, the repo's C++ as test oracle (*default*) |
| Q9 | Existing C++ AutoStory | Continues in parallel; port only after a pass-rate gate |
| Q10 | Slice order | S0 -> S1 -> S4 -> S2 -> S3; S0/S1 are the Mail Glitch checkpoints (revised, section 13) |
| Q11 | Reaching S4 | Port FRLG actions in the dex design's slice order, each gated against its C++ twin |
| Q12 | Simulation | In-process OCaml sim for execution; Python PokemonSimulation stays planner/evaluator, coupled by data only |
| D7 | OCaml layering | measurements -> facts -> logic; replay injects measurements, sim injects facts |
| Q13 | Program style | Direct-style kernel and actions; checkpoints and stories as typed data |
| D8 | Effects | Programs depend only on our own small effect set, never on an I/O library directly |
| Q14 | Agents/MCP | Extend pa-lab with a second channel adapter; `contracts/v1` is the spec |
| Q15 | Budget | Week 0 spikes, then S0+S1 in at most 5 weeks on one lane, with stop rules |

## 3. Layer 1: I/O

Contract: **frame source + input sink + clock**. Every frame gets a monotonic `seq` and a
capture timestamp; commands are timed against the same clock.

What the code offers today:

- `CoreLib` (`SerialPrograms/cmake/CoreLib.cmake`): 339 files, about 43k lines, no Qt,
  OpenCV, ONNX or Tesseract. The daemon includes upstream's cmake files for it unchanged. It contains the serial connection, the PABotBase2 protocol,
  controller scheduling and the Switch controllers, plus JSON and sockets. Already used by
  `SerialProgramsCommandLine`. Link it as is.
- Controller interface (`NintendoSwitch_ProController.h`, `Controllers/Controller.h`):
  `issue_buttons/dpad/joystick(delay, hold, cooldown)`, `issue_barrier`, `issue_nop`,
  `wait_for_all`, `cancel_all_commands`, `replace_on_next_command`. The API mirrors this.
- Video capture is entirely Qt Multimedia (`CommonFramework/VideoPipeline/Backends`). It is
  not reused. The daemon gets its own `FrameSource`, first through OpenCV `VideoCapture`
  (*unverified*; fallback is borrowing the Qt capture).
- Do not link `SerialProgramsLib`: it is the whole application.

Backends behind the same contract: Switch (capture card + PABotBase2), mGBA (the fork
already has an mGBA lane), recorded frames.

Audio capture is deferred; the slot is reserved as another probe stream.

## 4. Layer 2: probes

A **probe** is `{id, kind, box, params}` registered by the program. The daemon evaluates it
on frames and returns raw results with no judgment.

| Kind | Returns | Cost (rough, not measured) |
|---|---|---|
| `color_stats` | mean RGB, stddev over a box | microseconds |
| `rmsd_prev` | difference against the previous frame | about a millisecond |
| `template` | match score against a template at a box | milliseconds |
| `find` | fixed pipeline: colour filter -> waterfill -> size filter -> template; list of boxes with scores | milliseconds |
| `ocr` (on request) | text | tens to hundreds of milliseconds |
| `model` (on request) | boxes, labels, scores | tens of milliseconds |

The probes are written on OpenCV (`meanStdDev`, `absdiff`, `matchTemplate` or a port of
PA's RMSD formula, `inRange` + `connectedComponentsWithStats`). PA's image primitives
(`Kernels`, 129 files whose SIMD flags live in the main `CMakeLists.txt`, `ImageMatch`,
waterfill) are not compiled: carrying their source list and flags would be the heaviest
overlay. Only `CoreLib` comes from PA, through upstream's own `SourceFiles.cmake` and
`CoreLib.cmake`.

Invariants fixed from the start:

1. Every result is keyed by `(probe id, frame seq, capture time)`.
2. Two delivery classes: **sync** probes are bundled into one complete record per frame;
   **async** probes (OCR, models, later workers) arrive as their own message with their
   `seq`.
3. Per-probe policy: `latest` (dropping frames is fine) or `every` (run on each captured
   frame, buffer results, a gap is an error). BDSP blink timing needs `every`.
4. Kinds are namespaced and versioned (`core.color_stats@1`); an unknown kind is an error
   at subscribe time.
5. Capability handshake at connect; the program fails fast on a missing kind.
6. Closed set of result shapes: numbers, boxes (with score and label), text.
7. A recording is the wire messages themselves.

Composition uses the frame ring and `seq`, not a dataflow language: `find` on frame N, the
program computes a box, `ocr` that box on frame N.

Evolution, all through one concept (a **probe provider**): a new in-daemon kind; a derived
probe whose inputs are other probes' numbers (gating, daemon-side thresholds); an external
worker process fed frames by shared memory. The program sees no difference. Placement
rule: needs pixels, per frame, cheap -> daemon kind; needs pixels, heavy or its own stack
-> worker; needs no pixels -> OCaml. The program never sends code to the daemon.

In-process pipeline and the rule on each edge:

| Edge | Rule |
|---|---|
| capture -> probes | latest frame wins; a small ring is kept for `seq`-addressed reads |
| probes -> client | ordered, bounded; a slow client sees a gap in `seq`, never silent loss |
| client -> scheduler | ordered, reliable, never dropped |
| OCR / model requests | separate pool, request/reply, never stalls per-frame results |

When ML is worth it: only for a target with no fixed pixels, meaning an object in a 3D
world under a free camera. In the repo that is SV Area Zero navigation and one LZA hunt.
HUD, menus and 2D games use kinds 1-3. FRLG and BDSP do not need it.

## 5. Wire protocol

JSON-RPC 2.0, one message per line, localhost TCP. Calls: subscribe/unsubscribe probes,
submit command batch, cancel/replace, on-request reads. Notifications: per-frame records,
batch done, device or video lost. Frames are fetched by `seq` from a separate binary
endpoint. PA's core already has the JSON and socket code, so the daemon needs no new
dependency for this. MCP is also JSON-RPC 2.0, which keeps adapters thin.

## 6. Layer 3: OCaml programs

Layering:

```
measurements (probe numbers)     <- daemon client | replay of a recorded wire log
     │  per-game readers: numbers -> facts
facts ("dialog open")            <- simulation injects here
     │
logic (settle rule, walker, engine, planner)
```

One backend interface, three implementations; program code does not know which one runs.

Terms:

| Term | Meaning |
|---|---|
| Kernel op | one effect: `submit`, `await`, `race`, `now`, `read` |
| Reader | pure function from measurements to a fact |
| Action | one constructor of the checkpoint data type and the one function that runs it (the fork's C++ uses the same word) |
| Checkpoint | a list of actions that starts from a save and ends at a save |
| Story / controller | a sequence or generator of checkpoints |

```ocaml
let dialog_open m =
  let b = Probe.get m dialog_border in
  b.stddev < 12. && Color.near white b.mean

let heal_to_full () =
  route (To Center_counter);
  press A;
  settle ~quiet:(sec 5.) ~idle:(press B);
  expect Party_full_hp

let dex_025 =
  [ From_save Viridian_center; Mart [ Poke_ball, 20 ]; Heal
  ; Route { to_ = Viridian_forest_grass; mode = Run }
  ; Capture Pikachu; Dex_entry (Pikachu, Caught)
  ; Fly Viridian; Heal; Save ]
```

Why data for checkpoints: it matches rules already in force (events as data, no
hand-written legs, one state -> action table), and the dex collector must generate
checkpoints at run time.

D1 enforcement, set up when the repo is created: an architecture note stating the rule; a
lint that fails on `foreign_stubs`, `foreign_archives`, `extra_objects` or `ctypes` in any
`dune` file; a hand-maintained dependency allowlist; a Windows CI build.

Known costs: effects need OCaml 5 or later, and an unhandled effect is a runtime error, so
every action must be exercised under each handler in tests. The choice of I/O library
under the real handler is open (maturity of Eio on Windows is *unverified*) and hidden
behind D8.

## 7. Simulation and PokemonSimulation

The OCaml sim implements the backend interface in process with a virtual clock, for
sweeps, Monte Carlo and fault injection. PokemonSimulation (Python) keeps map generation
from the decomp, route scoring, battle evaluation on Showdown and catalog generation. The
two share data only: maps, the mechanics table, the acquisition catalog, plans and
policies, under PokemonSimulation's `schemas/`. Tile physics exists in both languages;
guard it with the shared mechanics file and a parity test (same route and inputs give the
same end tile).

## 8. Agents and MCP

pa-lab (`lab/server` on the `agents` branch; design in `docs/design/Agent-Integration.md`
on `lab/tooling`) stays the MCP front and control plane: leases, idempotent requests,
event cursor, audit, refusals. The app is already behind one channel adapter (fork #611);
the new stack is a second adapter. The daemon enforces only the minimal input lease. The
OCaml runner serves describe, run, stop, outcomes and save facts.

Carried over unchanged: three lease owners with reads never refused; exact-retry request
records; server preflight versus program start checks; typed terminal outcomes tied to the
Start request; fresh frames only; save facts; options by key; raw input as a leased,
debug-only tool.

No longer needed on this stack: the file channel, panel selection, option staging and
restore, the profile launch guard. Slice 7b's rejected "one console owner" becomes the
natural shape because there are no panels.

For the current pa-lab plan: finish what the C++ track needs (live verification of
structured outcomes, game identification); build save facts so the same JSON document can
be written by OCaml programs; do not invest further in retiring CFR on the old app.

## 9. Scope and slices

| Slice | What | Proves |
|---|---|---|
| S0 | Mail Glitch checkpoints (fork #781, `mg-01`..`mg-05`) on mGBA from their saves | probe model, stream, OCaml loop, protocol |
| S1 | The same checkpoints on console B's post-game save | capture, `CoreLib` link, real timing |
| S4 | FRLG dex collection (design: `autostory/frlg/design/AutoStory-FRLG-Dex-Collection.md` on `agents`, fork #776) | engine, planner, the FRLG action set |
| S2 | BDSP legendary shiny RNG | `every` probes, clock-anchored batches, OCaml RNG |
| S3 | SV programs (legendary collection, isolated shiny hunts) | free-camera 3D, model and audio probes |

S0/S1 are the first S4 slice: the Mail Glitch is Track 1A of the dex plan. The port starts
after #781 lands; its `MailGlitchActivation` is the C++ twin.

S4 on this stack: the catalog and maps are reused as data; snapshot, capabilities, planner
and checkpoint factory are OCaml; the bulk is the FRLG actions (`route`, Fly, `capture`,
`heal_to_full`, `mart`, `save`, `from_save`, `dex_entry`, fleeing wild battles), each with
a C++ twin on the fork except the Pokédex list reader. No story checkpoints are ported:
the C++ chain finishes the story to the Hall of Fame and hands over the save with team and
balls. The dex design's slices 2-3 (list inference in C++) need redirecting to probes.

S2: the repo already has the reference (`Pokemon_Xorshift128.h`, `Pokemon_BdspRng.h`,
`BlinkModel`, `StateSolver`, `StateReidentifier`, the seed finders, and blink extraction
that fits a tick period from per-frame timestamps). Generate golden vectors from it.

Gates: pass rate (10 runs each, both stacks from the same save, interleaved) on mGBA;
real timing on the Switch (3 testing-mode runs per checkpoint, no save, then one forward
run that saves); `mg-05` (the glitch itself) is gated on mGBA only.

## 10. Week 0, budget and stop rules

Week 0 spikes, with go/no-go:

| Spike | Question | If it fails |
|---|---|---|
| Capture | OpenCV holds 1080p at full rate and low latency from the lab card | borrow PA's Qt capture |
| Link | `CoreLib` plus OpenCV build into our own Qt-free binary on three OSes | carry a patched copy of the two cmake files |
| Latency | frame -> probe -> OCaml -> command on the wire, p99 under one frame (33 ms) | Q4 is wrong: stop and redesign before S1 (mGBA runs in lockstep, so S0 does not depend on it) |

Then S0 + S1 in at most 5 weeks on one lane; other lanes keep the C++ track moving.

1. A failed spike takes its fallback and a new estimate, reported before continuing.
2. S0 not green by the end of week 3: stop and write up why.
3. S1 gate: 10 runs each from the same save, pass rate at least the C++ twin's. On a miss,
   one week to fix, then stop.
4. Dev-loop gate: a wrong threshold is fixed and proven on a replay in under a minute, with
   no console and no C++ rebuild. If that is not true by S1, the stack has not earned S4.
5. More than 6 weeks in total: stop and reassess.

Estimates here are rough judgments, not measurements.

## 11. Defaults taken without discussion

- Operator side: CLI plus a config file first; a web view later; no settings UI.
- Assets: templates live in the program repo beside the reader that uses them.
- Recordings: the wire log plus a frame-ring dump on failure, under the existing lab disk
  GC.
- Repo: `dayfine/GamePilot`, private, with `daemon/` (C++) and `programs/` (dune),
  `contracts/` and `docs/`; Arduino-Source (fork `main`) and Packages as read-only
  submodules. Nothing is edited inside a submodule; fixes go to the fork and arrive by a
  bump.

## 12. Open

- I/O library under the real effect handler: settled (stdlib, section 13).
- Repo, S0/S1 checkpoint, mGBA attachment, console takeover: settled (section 13).
- Exact console-B windows for S1: decided by the main session under the one-train rule.

## 13. Second interview (2026-11-22)

| # | Question | Decision |
|---|---|---|
| R1 | Primary goal | The dev loop: freedom from Arduino-Source's build drag and upstream restrictions. Stop rule 4 is the main gate; the C++ pass rate is a "no worse than" floor |
| R2 | Upstream | Nothing has to go upstream. The overlay on Arduino-Source stays thin |
| R3 | Image primitives | Probes on OpenCV; only `CoreLib` from PA via upstream's cmake files |
| R4 | S0 backend | mGBA through the existing `lab/mgba/bridge.lua` protocol, unchanged (the daemon is a second client), plus replay of the recorded wire log |
| R5 | S0/S1 checkpoints | The Mail Glitch checkpoints of fork #781, after it lands. mGBA has a save per checkpoint (`~/FRLGLab/mgba/emu-1/saves`, story to `20.05`; `mail-glitch-test/` for `mg-*`); console B is post-game |
| R6 | Repo | `dayfine/GamePilot`, private, `~/Projects/GamePilot`; harness from `dayfine/agent-harness` plus the fork's `pr-maintenance`, `reviewer`, `dispatch`, `console-iteration`, `codex-dispatch`, `cpp-style-review` |
| R7 | C++ | One small daemon, glue only (about 2k lines): no thresholds, state machines or game names. `daemon/` and `programs/` are fully separate, each with its own build, lint and CI; `clang-format`, strict warnings, PA's `Filesystem` wrappers for paths |
| R8 | Real effect handler | OCaml stdlib only (`Unix` sockets, one reader thread, a queue, a small scheduler); OCaml 5.2+ pinned in CI on three OSes; Eio later is a handler swap (D8) |
| R9 | Gates | Pass rate on mGBA; timing on the Switch in testing mode; `mg-05` on mGBA only |
| R10 | Console B | GamePilot is one more lab process under the one-train rule (`lablock.sh`); the main session schedules windows |
| R11 | Swappable parts | P1: anything that may be swapped (I/O backend, RNG, effect handler, probes) sits behind one boundary. No I/O survey or RNG library search up front |
| R12 | FRLG RNG | Needed in S4; D6's OCaml rewrite starts there, PA's FRLG RNG as oracle |
| R13 | Templates | Packages as a read-only submodule; new templates in `programs/<game>/assets/`; programs upload templates at probe registration (cached by hash); template readers get replay tests |
| R14 | Manual control | pa-lab's MCP tools, mapped onto daemon calls by its second adapter, plus a hold state on the lease; no separate CLI |
| R15 | Defaults | D3 strict; game identification is an OCaml reader run by `ensure_game`; the latency spike gates S1, not S0 |
