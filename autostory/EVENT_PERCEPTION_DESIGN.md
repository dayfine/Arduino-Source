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

## Prior art (checked 2026-11-08)

Neither SV AutoStory (upstream main) nor Butter's BDSP AutoStory
(upstream #1717) has a timeline or event bus. Both attach several watchers
to the move itself with `run_until` and act on whichever fires first:

- SV `overworld_navigation`: battle, dialog and marker watchers around the
  whole walk, and a nested `run_until` on `NoMinimapWatcher` (cheap) that
  stops the stick and then waits for the battle or dialog. That is a
  detector gated on another. `do_action_and_monitor_for_battles` /
  `_for_overworld` wrap any action the same way.
- #1717: black OR white OR battle watchers around a stick hold
  (Segment 03); for the Escape Rope it starts the watchers *before* the
  press, "so the transition cannot happen before the watcher starts". Our
  Gate failures are exactly that gap. Fades use the shared
  `BlackScreenWatcher` / `WhiteScreenWatcher` (rgb sum < 100 and stddev < 10
  over 80% of the screen): a textured dim floor fails the stddev test, so no
  relative-fade logic is needed if real fades are that uniform.
- Position: #1717 matches landmark templates (`OverworldMatchers`, dx/dy
  from a sign or door); SV realigns from the minimap marker and YOLO.
- Recovery: SV's `checkpoint_reattempt_loop` reloads and retries the
  checkpoint; tolerance of one missed signal here is on top of that, not
  instead of it.

Consequence: slice 1 needs no new infra, only `run_until` with several
watchers around the move. The timeline (slice 2) is new to this codebase;
build it only if "the walker reports what it saw" (below) falls short.

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
2. **Walkers report warps.** Every walk attaches the fade and banner
   watchers, as #1717 does, and returns `WARPED` (with the banner text, if
   read) like it returns a battle. The next step checks that report instead
   of waiting for a fade that has passed. Red test: an open leg that slides
   onto a warp (from the P3 rule's sim). A run-wide `ScreenTimeline` only if
   this proves too narrow.
3. **Uniform fade**: use the shared watchers' stddev test as #1717 does,
   not mean brightness < 75. Ours run loose (rgb sum 225, stddev 60) because
   cave fades measured at 45 to 64 brightness; check them against the Gate
   frames (`Screenshots/AutoStory/banner/`) before tightening. Red test:
   start inside the dim Gate.
4. **Talk-over from events** (post-battle speeches, scenes): replaces the
   ad hoc settle loops, including today's `settle_after_battle`.
5. **FRLG binding**: its watchers post to the timeline, and its arrival
   checks read it. Shared tests from both games.

## Demo (pseudo code)

Names follow the BDSP code on #226 (`walk_until`, `WalkResult`,
`location_banner_showing`, `read_location_banner`). Not compiled.

**What the walker already does.** `WalkRunner::hold()`
(`Utils/PokemonBDSP_AutoStory_Walk.cpp`) runs every chunk under
`run_until` with five watchers: encounter flash (white), `BlackScreenWatcher`
(strict 100/10), battle menu, dialog, battle text. A black fade stops the
walk as `WalkStop::TRANSITION`, and the route then fails with "unexpected
transition". Two gaps follow from that:

- On 16:03 the Gate legs walked on without a TRANSITION: the Gate's fade
  didn't pass the strict black test (or fell between chunks).
- A white fade (cave exit) is caught by the *encounter flash* watcher and
  read as a wild battle.

### 1. A banner watcher: cheap gate, expensive read

```cpp
//  Reads the banner text at most twice per appearance: when the box shows,
//  and once ~300 ms later in case the first frame caught it sliding in.
class BannerWatcher : public VisualInferenceCallback{
public:
    BannerWatcher(std::string expected, bool stop_on_match);

    bool process_frame(const ImageViewRGB32& frame, WallClock now) override{
        if (!location_banner_showing(frame)){      //  colour test, every frame
            m_reads = 0;                            //  next appearance reads again
            return false;
        }
        if (m_reads >= 2 || (m_reads == 1 && now - m_first_read < 300ms)){
            return false;
        }
        if (m_reads == 0){
            m_first_read = now;
        }
        m_reads++;
        const std::string read = read_location_banner(frame);   //  Tesseract
        m_last_read.store(read);
        if (!location_banner_matches(read, m_expected)){
            return false;
        }
        m_seen.store(now);
        return m_stop_on_match;
    }

    std::optional<WallClock> seen() const;
    std::string last_read() const;
};
```

### 2. A fade watcher that tells black, white and an encounter apart

```cpp
//  Black or white over 80% of the screen, uniform (the stddev test the
//  shared watchers use, as in #1717), held >= 150 ms and then gone. An
//  encounter is several short white flashes; a cave exit is one long white.
class FadeWatcher : public VisualInferenceCallback{
public:
    enum class Kind{ NONE, BLACK, WHITE };
    explicit FadeWatcher(bool stop_when_over);
    bool process_frame(const ImageViewRGB32& frame, WallClock now) override;
    Kind kind() const;
    std::optional<WallClock> over() const;     //  when the fade cleared
};
```

Thresholds come from today's Gate frames (`Screenshots/AutoStory/banner/`)
before code: the rgb sum and stddev of the darkest Gate fade frame, and of
the Gate floor, with a margin between them.

### 3. The walker reports what it saw (slice 2)

```cpp
struct WarpSeen{
    FadeWatcher::Kind fade = FadeWatcher::Kind::NONE;
    std::optional<WallClock> fade_over;
    std::string banner;                  //  matched text, empty if none
    bool any() const{
        return fade != FadeWatcher::Kind::NONE || !banner.empty();
    }
};

struct WalkResult{
    WalkStop stop;
    //  ...existing fields...
    WarpSeen warp;                       //  new: filled by every walk
};

int WalkRunner::hold(Milliseconds chunk){
    WhiteScreenOverWatcher encounter_flash(...);
    FadeWatcher fade(/*stop_when_over*/ true);              //  replaces `black`
    BannerWatcher banner(m_spec.expected_banner, /*stop*/ false);   //  observe only
    BattleMenuWatcher battle_menu(...);
    ShortDialogWatcher dialog(...);
    BattleDialogWatcher battle_dialog(...);
    const int ret = run_until<ProControllerContext>(m_stream, m_context,
        [&](ProControllerContext& context){
            press(context, chunk);
        },
        {{encounter_flash}, {fade}, {battle_menu}, {dialog}, {battle_dialog}, {banner}}
    );
    m_result.warp.merge(fade, banner);
    return ret;
}
```

`expected_banner` is the next map on the plan (the leg's `map_after`, or
the following step's far map). A banner for any other map is logged, not
matched.

### 4. Route leg: a warp mid-leg hands over instead of failing

```cpp
//  PlannedRoute.cpp, after a leg
if (walk.result.stop == WalkStop::TRANSITION || walk.result.warp.any()){
    if (location_banner_matches(walk.result.warp.banner, m_plan.next_map)
        || (walk.result.warp.fade != FadeWatcher::Kind::NONE && m_plan.ends_at_warp)
    ){
        m_io.log("[Route] slid onto the warp during " + walk.label + "; handing over.");
        m_io.record_warp(walk.result.warp);
        return LegNext::ARRIVED_EARLY;
    }
    fail(m_io, walk.label + ": unexpected transition.");
}
```

### 5. Door step: fade OR banner, and check the hand-over first (slice 1)

```cpp
void step_into(StepIO& io, Direction dir, const std::string& far_map){
    //  The previous leg may already have gone through.
    if (io.last_warp().matches(far_map)){
        io.log("[Door] already through to " + far_map + " (" + io.last_warp().describe() + ").");
        return settle_short(io);
    }

    //  Watchers start before the press, so the warp can't happen unwatched.
    FadeWatcher fade(/*stop_when_over*/ true);
    BannerWatcher banner(far_map, /*stop*/ true);
    const int ret = run_until<ProControllerContext>(io.stream(), io.context(),
        [&](ProControllerContext& context){
            run_dpad(context, dir, 6000ms);       //  P2: run, ends at the warp
            pbf_wait(context, 4000ms);
        },
        {{fade}, {banner}}
    );
    if (ret < 0){
        fail(io, "no fade and no " + far_map + " banner; last read \"" + banner.last_read() + "\".");
    }
    if (ret == 0){
        //  Fade first. The banner usually follows ~1.5 s later: wait for it
        //  as evidence, but don't require it.
        if (wait_until(io.stream(), io.context(), 3000ms, {{banner}}) < 0){
            io.log("[Door] fade, no banner: counting the fade.", COLOR_ORANGE);
        }
    }
    settle_short(io);       //  P2: ~1 s quiet overworld unless the step flags a scene
}
```

### 6. Sim gate

```cpp
//  GridSim posts the same WarpSeen a console walk would. The seeded
//  disturbance drops one of the two signals at each warp.
sim.on_warp = [&](WarpSeen& seen){
    if (rng.chance(0.5)){
        if (rng.coin()){
            seen.fade = FadeWatcher::Kind::NONE;
        }else{
            seen.banner.clear();
        }
    }
};
//  Red today (19/20), green with slice 1:
CHECK(monte_carlo_pass_count("01.05", 20) == 20);
//  Slice 2: a leg that slides onto the Gate's west mouth ends the route
//  early, and out_of_oreburgh_gate passes from the hand-over.
CHECK(run_row("01.05", Overshoot{"out_of_oreburgh_gate", 1}).passed);
```

## Non-goals

- No new thread pools or async runtime: observers are inference callbacks on
  the existing video pipeline.
- No change to what a step *means*; only to how it decides it happened.
- No per-frame OCR: text reads are gated by a cheap detector (see Cost).
- Not a rewrite of the walkers: they keep their stop watchers. Observers
  come on top of them.
