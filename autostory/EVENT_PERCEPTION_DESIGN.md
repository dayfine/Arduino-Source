# Event-driven perception for AutoStory (design note, P1)

Plan item P1 in `UNIFICATION_PLAN_2026-11-08.md` (re-prioritized section).
Short by intent: the shape, the seams and the first slices. Implementation
goes to a code agent, slice by slice, each with sim tests from both games.

## Problem

Each step polls one detector in a window it owns. What it doesn't watch, it
never learns. Console B, 2026-11-08:

| Failure | Cause |
|---|---|
| Oreburgh Gate, both ways: `no_banner` | An open leg slid onto the warp; the fade and the banner happened inside the *route* step, which wasn't looking. The banner step then started on the far side. |
| Same steps | The banner step's "fade" test (`brightness < 75`) fired on the Gate's dim floor (46), so it waited for a banner already gone. |
| 01.05 Monte Carlo 19/20 | One injected missed fade at a door fails a real arrival: the door step accepts only the fade. |
| 01.04 at night | A single brightness threshold measured by day (street 104) failed at night (88). |
| Roark's badge speech | "No box for a moment" was read as "the talk is over" during the fanfare pause. |

A single signal judged in a single window is fragile. Several signals,
recorded all the time and judged together, aren't.

## Shape

**1. Observers run during every controller action, not per step.**
A set of passive observers watches every frame while the program moves,
waits or talks. Kinds:

- fade (black or white): entry and exit, with the brightness drop relative to
  the frames before it, so a dim room isn't a fade;
- location banner, with its text;
- dialog box present or absent;
- battle start;
- screen still or moving.

They're `VisualInferenceCallback`s, the same mechanism the shared
`LegWalker`'s `LegInterrupt` watchers and BDSP's `walk_until` already use, so
no new threads (AGENTS.md: no `std::thread`). The difference from
`LegInterrupt`: they don't stop the action. They only record.

**Cost: cheap gates, expensive reads.** The framework samples every 50 ms
(`InferenceRoutines.h`). Fade, dialog, battle and still tests are color or
brightness checks on small boxes and run on every frame. Expensive reads run
only when a cheap detector fires: the banner's text (Tesseract, tens of ms)
is read once when the banner box appears, plus one confirming read about
300 ms later, and not again until the box goes away. Today `look_after_tap`
runs OCR on every poll while the box shows (30 to 40 reads per banner).
Frame dumps (the 500 ms banner shots) stay a debug option, not a recorder.

**2. They post timestamped events to one timeline.**
`ScreenTimeline`: an append-only list of `{time, kind, detail}` (e.g.
`{14:03:50.4, FADE_WHITE}`, `{14:03:51.9, BANNER, "Route 203"}`), owned by the
run, living across steps. Every step records where the timeline was when it
started.

**3. Steps judge from events, not from their own polls.**
A step asks the timeline, for example:

- Did we reach map M since step start? Yes if any one of:
  - a fade, then frames that fit M;
  - M's banner;
  - a position that fits M's map, once the walker can localize.
- Is the talk over? The overworld is back and no box has shown for N s, with
  the latest box event as the clock, not "no box right now".

Arrival through a warp becomes *fade or banner or position*, any one of
them. One missed signal no longer fails a real arrival, and a signal that
came early (during the previous step) still counts.

**4. Thresholds are relative and cross-checked.**
A fade is a drop against the recent frames plus near-black or near-white, not
one absolute number. A threshold used for a decision needs a second signal
that agrees, or it logs that it is deciding on one.

## Seams (both games)

- **Shared, in `Pokemon/AutoStory` (#122):** `ScreenTimeline` (data and
  queries) and the arrival and talk-over judgments, game-agnostic.
- **Per game:** the observer set. BDSP: banner box, fade, dialog. FRLG: its
  existing BattleFlash, BattleScreen and WhiteDialog watchers plus the
  map-name popup.
- **Walkers:**
  - BDSP's `walk_until` / `walk_until_dpad` and FRLG's `LegWalker` run the
    observers alongside their own stop watchers.
  - `StoryStepIO` exposes the timeline to the executor.
- **Sims:**
  - `GridSim` / `StorySim` (BDSP) and `OverworldSim` (FRLG) post the same
    events from simulated warps, banners and talks.
  - The seeded disturbance sim (#262, B6) drops or delays single events.
  - **Gate:** a row passes with any single event missed, early or late.

## Slices

1. **Door arrival = fade or banner** (BDSP first). The door step watches both
   through its run and the settle, and accepts either one. Red test: 01.05's
   Monte Carlo, one missed fade at the Gate gives 19/20 today; it must give
   20/20. It also unblocks the Gate exit as one run west.
2. **`ScreenTimeline` in the shared engine**, fed by BDSP's walkers; banner
   steps read it, so a banner seen during the previous step counts. Red
   test: an open leg that slides onto a warp (from the P3 rule's sim).
3. **Relative fade**: drop against recent frames, not a fixed 75. Red test:
   start inside a dim cave.
4. **Talk-over from events** (post-battle speeches, scenes): replaces the
   ad hoc settle loops, including today's `settle_after_battle`.
5. **FRLG binding**: its watchers post to the timeline, and its arrival
   checks read it. Shared tests from both games.

## Non-goals

- No new thread pools or async runtime: observers are inference callbacks on
  the existing video pipeline.
- No change to what a step *means*; only to how it decides it happened.
- No per-frame OCR: text reads are gated by a cheap detector (see Cost).
- Not a rewrite of the walkers: they keep their stop watchers. Observers
  come on top of them.
