/*  BDSP AutoStory - Story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story table's rows run as the program runs them (the segment
 *  runner, make_story_segment()), with the program's own walking steps
 *  (StoryWalkSteps) on the grid simulation; see StorySim_Tests.cpp for
 *  what is simulated and what is stubbed. Moved out of the tests so the
 *  console failure cases (SimFailureCases_Tests) run on the same executor.
 *  With a SimDisturbance the run is disturbed as the console disturbs it
 *  (wild battles, drift off the grid, people wandering): the story's Monte
 *  Carlo tests.
 *
// code-health: allow header_symbols -- story simulation executor, scenario runners and mock IO
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StorySim_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StorySim_H

#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_TalkOver.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryActions.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryInterrupts.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkResult.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkScreen.h"
#include "PokemonBDSP_AutoStory_GridSim.h"
#include "PokemonBDSP_AutoStory_SimDisturbance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Which detections the simulation misses on purpose (#162): the `fade`th
//  door fade (run_into_door(), from 0) or the `banner`th map banner
//  (run_until_map()) of the run goes unseen, though the player did go
//  through. The console missed both (r75, r81). The `hold`th fade (counted
//  with `fade`'s) shows its fall and the scene after but not its hold: a
//  short fade's hold between two frames. The `dark`th door run reads
//  dark detail::SIM_FALSE_DARK in, short of the door, as lab-B's did on 2026-11-08.
//  The `through`th tap step (tap_to_banner(), from 0) starts on its far side,
//  its banner gone unless `through_banner`: the walk before it slid onto the
//  warp (lab-B 2026-11-08). The counts and flags below are the run's: how
//  many fades and banners it looked for, whether a miss happened since the
//  executor last looked, whether a missed fade is still unseen, whether a
//  walk slid on (SimSlide) since the executor last looked, and the map whose
//  banner a slide left up for the next step.
struct SimMisses{
    std::optional<size_t> fade;
    std::optional<size_t> banner;
    std::optional<size_t> hold = {};
    std::optional<size_t> dark = {};
    std::optional<size_t> through = {};
    bool through_banner = false;
    size_t tap_steps = 0;
    size_t fades = 0;
    size_t banners = 0;
    size_t doors = 0;
    bool missed = false;
    bool fade_unseen = false;
    //  A door's fade showed and nothing has settled since: on the console
    //  the inside is still fading in, and a walk then goes nowhere (lab-B
    //  2026-11-10, 00.04b heal_sandgem: "nurse not reached").
    bool fading_in = false;
    bool slid = false;
    std::string banner_up = {};
};

//  How a warp's fade looks on the screen (#268's design note); the
//  program's fade_of() judges it, as the walk's watchers do.
enum class SimFade{
    //  A door's: near black (SIM_DOOR_FADE).
    BLACK,
    //  A cave mouth's: dim, not black (SIM_DIM_FADE). The walk's old strict
    //  black watcher (rgb sum 100) missed it and the walk went on (the
    //  Oreburgh Gate, lab-B 2026-11-08 16:03).
    DIM,
    //  A cave exit's white. The walk's old encounter-flash watcher read it
    //  as a wild battle.
    WHITE,
};

//  An open leg that runs on past its end: the walk taps of the step `step`
//  toward `direction` go `tiles` further, once per step. Onto a warp, the
//  player lands on `far` (the sim's maps have no warps' destinations), the
//  walk sees the fade as `fade` looks, and the far map's banner is still up
//  for the next step (`banner_up`) or gone (01.05, lab-B 2026-11-08 15:31).
struct SimSlide{
    std::string step;
    Pokemon::Direction direction = Pokemon::Direction::WEST;
    size_t tiles = 1;
    Pokemon::Pose far = {};
    SimFade fade = SimFade::DIM;
    bool banner_up = false;
};

namespace detail{
//  One beat of a scripted scene: a line, up until a press, or `length` of
//  screen with no box, `overworld` or not (a battle's end, a fade),
//  `moving` (someone walking, the camera panning) or still, faded as `fade`.
//  `at_player`: the moving reaches the box around the player.
struct SimBeat{
    std::string what;
    bool line = false;
    Milliseconds length = Milliseconds(0);
    bool overworld = true;
    bool moving = false;
    bool at_player = false;
    WarpFade fade = WarpFade::NONE;
};
//  A line, up until a press.
SimBeat sim_line(std::string what);
//  `length` of overworld with no box, someone moving or not.
SimBeat sim_pause(std::string what, Milliseconds length, bool moving);
//  `length` of overworld with someone walking right past the player.
SimBeat sim_walk_past(std::string what, Milliseconds length);

//  `length` of a fade, as BDSP's eyes see it: no box, so the overworld,
//  and still (a black frame matches the one before).
SimBeat sim_fade(std::string what, Milliseconds length, WarpFade fade = WarpFade::BLACK);
//  A talk or a scene as the screen shows it, beat by beat; the quiet
//  overworld after the last beat.
using SimScene = std::vector<SimBeat>;
//  Plays `scene` against the program's TalkOver with `rule`, a look per
//  press, as talk_over() takes them: "" if it is judged over after the last
//  beat, else when it was judged over early, or that it never was.
std::string play_scene(const SimScene& scene, const TalkOverRule& rule);
} // namespace detail

//  What the console showed that the plain simulation leaves out, each off
//  unless set. All of it is deterministic: wanderers walk from `wander_seed`.
struct SimWorldKnobs{
    std::vector<SimSlide> slides = {};
    //  The mean brightness (0 to 255) of a map's floor, by JSON map name, and
    //  of the fade into a map; a tap step judges its fade from these with the
    //  program's fade_of(), a floor textured, a fade uniform (SIM_FLOOR_STDDEV,
    //  SIM_FADE_STDDEV). Unset: 104 (Oreburgh's street by day) and 7 (a
    //  door's black).
    std::map<std::string, double> floor = {};
    std::map<std::string, double> fade_into = {};
    //  People with a wander box on their map walk it, a step tried every 2 s.
    std::optional<uint64_t> wander_seed = std::nullopt;
    //  People with a wander box who stand still on a tile of their map
    //  instead, by name, with or without wander_seed: 00.05b's boy below
    //  the Mart (school_kid_m_5, x 51, y 33..35) on row 34.
    std::map<std::string, Pokemon::TilePos> standing = {};
    //  How much longer a hold into someone standing still takes to read as
    //  stopped: the screen moves on while we push against them. On lab-B
    //  (2026-11-11 21:00, 00.05b) "east 8 (to wall)" stopped against the boy
    //  below the Mart after 2800 ms, about 5 tiles in, where a free run
    //  stops at its wall after about 2000.
    Milliseconds pushing_someone = Milliseconds(0);
    //  How long after the A press a talk's first line shows, by step name.
    std::map<std::string, Milliseconds> first_line = {};
    //  The scene a step's talk-over plays, by step name: a scene's or a
    //  door's settle(), or the speeches after a battle step's battle.
    std::map<std::string, detail::SimScene> scenes = {};
    //  Whether a dialog pops up right before row `row`'s step `step`:
    //  someone walks up and talks between two steps. Until it is pressed
    //  through, it stops a step that walks or talks, as on the console.
    std::function<bool(const std::string& row, const std::string& step)> dialog_before = nullptr;
    //  Whether a battle's opening is still playing when row `row`'s step
    //  `step` starts: the encounter's white, no battle yet (00.02's Starly
    //  battle right after the starter is picked, lab-B 2026-11-09).
    std::function<bool(const std::string& row, const std::string& step)> battle_opening_before = nullptr;
    //  Whether the executor clears what came up between steps
    //  (story_interrupts()), as the console does; off, as before it did.
    bool clear_interrupts = true;
    //  A seed's disturbances (wild battles, drift off the grid, people
    //  wandering), which must outlive the run: the story's Monte Carlo tests.
    //  Within half a tile of where a step should end counts as there.
    SimDisturbance* noise = nullptr;
    //  Where a step really leaves the player, by step name, when that is
    //  not the table's pose_after or the walk's end: the program goes on
    //  from the table's pose, the player from here (00.03's scene a row
    //  low, lab-B 2026-11-09; 00.04b's run into Sandgem two columns west,
    //  lab-B 2026-11-09 20:09).
    std::map<std::string, Pokemon::Pose> lands = {};
    //  Wild battles in tall grass on every pass (SimGrassBattles); none unset.
    SimGrassBattles grass = {};
    //  Whether the scene after row `row`'s walk `step` starts early, on the
    //  walk's last tiles (its target): its trigger lies on the way, so the
    //  walk runs into it (00.04's Mom's call on (6, 9), lab-B 2026-11-09 17:29).
    std::function<bool(const std::string& row, const std::string& step)> scene_at_walk_end = nullptr;
    //  Every warp's fade-in runs long: a walk, a tap or a door run after a
    //  door's fade or a tap step's way through, with no settle() between,
    //  goes nowhere (lab-B 2026-11-10, 00.04b heal_sandgem walked to the
    //  nurse 1500 ms after the Center's fade and stopped on "transition").
    bool slow_fade_in = false;
    //  Where the first row's save really leaves the player, when that is not
    //  its declared start: the program starts from the declared start, the
    //  player from here (00.05, lab-B 2026-11-11: two tiles off).
    std::optional<Pokemon::Pose> start_at = std::nullopt;
    //  Jubilife's coupons (00.05b, 00.05c): the clown talks, by step name,
    //  whose coupon the save holds at the run's start; the clown talks that
    //  read another line first and give none (lab-B 2026-11-11: the
    //  Poketch Company's sign); and whether the save has the Poketch. The
    //  president's talk gives it with all three coupons only.
    std::set<std::string> coupons_at_start = {"mart_clown", "tv_clown", "company_clown"};
    std::set<std::string> talks_without_coupon = {};
    bool poketch_at_start = true;
};

namespace detail{

//  The walker's RouteIO on the story simulation, with SimWorldKnobs' slides
//  and late first lines, disturbed by SimWorldKnobs::noise if set. `step`
//  names the step running now; `cross` puts the player on a pose past a warp.
class SimKnobRouteIO : public DisturbedRouteIO{
public:
    SimKnobRouteIO(
        GridSim& sim, std::vector<std::string>& notes, SimMisses& misses, const SimWorldKnobs& knobs,
        const std::string& step, std::function<void(const Pokemon::Pose&)> cross
    )
        : DisturbedRouteIO(sim, notes, knobs.noise, knobs.grass)
        , m_grid(sim)
        , m_misses(misses)
        , m_knobs(knobs)
        , m_step(step)
        , m_cross(std::move(cross))
    {}

    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override;
    virtual WalkResult walk_taps(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual bool dialog_within(Milliseconds wait, Color color) override;
    //  With SimWorldKnobs::slow_fade_in, fails the move `label` if the last
    //  warp is still fading in (SimMisses::fading_in).
    void check_fade_in(const std::string& label);

private:
    WalkResult slide_on(const SimSlide& slide, WalkResult result);

    GridSim& m_grid;
    SimMisses& m_misses;
    const SimWorldKnobs& m_knobs;
    const std::string& m_step;
    std::function<void(const Pokemon::Pose&)> m_cross;
    std::set<std::string> m_slid_steps;
};
} // namespace detail

namespace detail{

//  How many tiles a held walk goes on into a map before its location
//  banner reads: "it shows a tile or a few past the border" (the table's
//  route_201_by_the_path(), measured on the console). Standing still, it
//  reads at once. Through a cave mouth the fade comes first, and a hold
//  whose fade went unseen goes on the same way (r75, r81).
constexpr size_t SIM_BANNER_TILES = 3;
//  How far into a door run SimMisses::dark reads dark, short of the door.
constexpr Milliseconds SIM_FALSE_DARK = Milliseconds(1800);
//  Brightness (0 to 255) of a floor where SimWorldKnobs does not say:
//  Oreburgh's street by day (r74).
constexpr double SIM_FLOOR = 104;
//  Brightness of a fade where SimWorldKnobs does not say: a door's black (by hand: 7).
constexpr double SIM_DOOR_FADE = 7;
//  Brightness of a cave mouth's dim fade (45 to 64 measured).
constexpr double SIM_DIM_FADE = 55;
//  Brightness of a cave exit's white fade.
constexpr double SIM_WHITE_FADE = 250;
//  The spread (stddev a channel) of a fade: uniform. A model, not measured
//  on the console.
constexpr double SIM_FADE_STDDEV = 1;
//  The spread of a floor, which the game draws with texture. A model too.
constexpr double SIM_FLOOR_STDDEV = 20;
//  The screen's middle as fade_of() reads it: `brightness` a channel, `stddev` a channel.
ImageStats sim_screen(double brightness, double stddev);
//  The frames a warp's fade shows, 33 ms apart (the console's 30 fps): the
//  floor (`floor`, textured), the fall, the hold of `fade` (uniform) and the
//  rise, and the floor on the far side. Without `hold_seen` only the fall
//  shows between the floors: a short fade's hold fell between two frames.
std::vector<Pokemon::TransitionSample> sim_fade_frames(double floor, double fade, bool hold_seen);
//  The fade the program's WarpFadeWatch with `rule` sees in those frames;
//  each scene after a fall is another (the far side's).
WarpFade sim_watch_fade(const WarpFadeRule& rule, double floor, double fade, bool hold_seen);
} // namespace detail

//  What a run on the simulation did.
struct SimRun{
    //  The first failure, as "row outcome", or "".
    std::string error;
    //  The rows and steps where an injected miss happened and where the run
    //  first failed, as "row step": "" if none.
    std::string missed_step;
    std::string failed_step;
    //  Walking steps that end where nothing says, as "row step".
    std::vector<std::string> unchecked;
    //  Each row run, with the fades and banners looked for before it, and
    //  its steps the simulation did not play (stub()), in order.
    struct RowStart{
        std::string row;
        size_t fades;
        size_t banners;
        std::vector<std::string> stubbed = {};
    };
    std::vector<RowStart> rows;
    //  Where the player stood after the last step, if placed.
    const StoryWorld* world = nullptr;
    Pokemon::TilePos end;
    //  Wild battles in the run, and in the step it failed in.
    size_t battles = 0;
    size_t failed_step_battles = 0;
    //  How the run failed: the failure and its message.
    std::string failure;
    std::string failure_detail;
    //  Scenes a walk took for a trainer's and pressed through, as
    //  "row step: scene" (the console's unknown-trainer handler, lab-B
    //  2026-11-09 17:29: Mom's call, then her scene's fade missed).
    std::vector<std::string> scenes_fought = {};
};

//  StoryStepIO on the simulation. A walk through a door or a cave mouth
//  stops on its tile, as the console stops at the fade; a hold that misses
//  the fade carries on, onto `far_side()` (the step's pose_after: the
//  sim's maps have no warps' destinations) and on along it.
class SimStoryStepIO : public StoryStepIO{
public:
    SimStoryStepIO(
        GridSim& sim, std::vector<std::string>& notes, SimMisses& misses, const SimWorldKnobs& knobs,
        const std::string& step, std::function<bool()> far_side, std::function<void(const Pokemon::Pose&)> cross
    )
        : m_sim(sim)
        , m_noise(knobs.noise)
        , m_route(sim, notes, misses, knobs, step, std::move(cross))
        , m_misses(misses)
        , m_knobs(knobs)
        , m_far_side(std::move(far_side))
        , m_step(step)
    {}

    virtual RouteIO& route() override{
        return m_route;
    }
    //  A scene's dialogue fought as a trainer's is pressed through under the
    //  walk, not its own step: noted (sim_fight()).
    virtual bool fight(const std::string&) override{
        detail::sim_fight(m_sim);
        return true;
    }
    virtual void walk_until_stopped(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual bool run_until_map(Pokemon::Direction direction, Milliseconds limit, const std::string& map, const std::string&) override;
    //  A door's fade stops the run; with the fade missed, the run goes on
    //  holding and sees the far map's `banner` show after it, if it has one
    //  (story_arrival_banner(): the sim's maps have no banners of their own).
    virtual WarpSeen run_into_door(Pokemon::Direction direction, bool run, Milliseconds timeout = Milliseconds(12000),
        Milliseconds dark_from = Milliseconds(0), const std::string& banner = "") override{
        m_route.check_fade_in(std::format("{}: run into the door", m_step));
        const SimPace pace = run ? SimPace::RUN : SimPace::WALK;
        if (false_dark() && dark_from <= detail::SIM_FALSE_DARK && detail::SIM_FALSE_DARK < timeout){
            disturbed_hold(m_sim, m_noise, direction, detail::SIM_FALSE_DARK, pace);
            return {.fade = WarpFade::BLACK};
        }
        if (disturbed_hold(m_sim, m_noise, direction, timeout, pace).move.stop != SimStop::WARP){
            return {};
        }
        const WarpSeen seen = door_fade(banner);
        m_misses.fading_in = seen.any();
        return seen;
    }
    //  A fade missed on the way in stays missed: it was over before settle()
    //  looked, which then fails as it does on the console after 120 s. The
    //  run before looked for the banner (run_into_door()), so settle() sees
    //  none here. With a scene in SimWorldKnobs::scenes, the program's
    //  TalkOver plays it instead.
    virtual void settle(Button, const std::string& label, SettleKind kind, const std::string&) override;
    virtual std::optional<size_t> tap_to_banner(Pokemon::Direction direction, const std::string& map, size_t max_taps) override;

private:
    //  tap_to_banner()'s taps, before it marks the far side fading in.
    std::optional<size_t> taps_to_banner(Pokemon::Direction direction, const std::string& map, size_t max_taps);
    //  How the run shows its next fade (SimMisses): whole, its hold unseen,
    //  or not at all.
    enum class FadeShown{
        SEEN,
        HOLD_UNSEEN,
        UNSEEN,
    };
    FadeShown next_fade();
    //  The next fade, of brightness `fade` from the floor here, as the
    //  program's watch with `rule` sees it.
    WarpFade watched_fade(const WarpFadeRule& rule, double fade);
    //  What a door run that reached its warp saw (run_into_door()).
    WarpSeen door_fade(const std::string& banner);
    //  Whether this door run reads dark short of the door, or the run
    //  misses this banner (SimMisses).
    bool false_dark(){
        const bool dark = m_misses.dark == m_misses.doors++;
        m_misses.missed |= dark;
        return dark;
    }
    bool banner_missed(){
        const bool miss = m_misses.banner == m_misses.banners++;
        m_misses.missed |= miss;
        return miss;
    }
    bool on_map(const std::string& map) const{
        return story_to_local(m_sim.world(), map, nearest_tile(m_sim.player().pos)).has_value();
    }
    //  The brightness of the floor the player stands on (SimWorldKnobs::floor).
    double floor_here() const;

    GridSim& m_sim;
    SimDisturbance* m_noise;
    detail::SimKnobRouteIO m_route;
    SimMisses& m_misses;
    const SimWorldKnobs& m_knobs;
    std::function<bool()> m_far_side;
    const std::string& m_step;
};

namespace detail{

//  A scene's trigger the simulation fires from row `row` on, once the step
//  `after` has run ("" from the row's start), on `map`'s `tiles`, while its
//  story variable `var` holds the value it waits for.
struct RowTrigger{
    std::string row;
    std::string after;
    std::string map;
    SimTrigger trigger;
    std::string var = {};
};

//  When a story variable holds the value its coord events wait for: from
//  row `row` on, once the step `after` has run ("" from the row's start).
struct VarState{
    std::string var;
    int value;
    std::string row;
    std::string after;
};

//  The story variables whose coord events the table walks onto, and from
//  which row and step each holds (the definition says why each).
std::vector<VarState> story_vars();
//  The maps' coord events whose story variable holds, as triggers.
std::vector<RowTrigger> story_triggers();
} // namespace detail

//  The program's walking steps on the simulation, the rest stubbed: the
//  story's action table (story_action_table()), its rows that need the
//  console bound to the stand-ins below.
class SimExecutor : public Pokemon::TableExecutor{
public:
    //  The table and the guard are members, built after the base: it only
    //  keeps references to them until a step runs.
    explicit SimExecutor(Logger& logger, SimMisses misses = {}, SimWorldKnobs knobs = {})
        : Pokemon::TableExecutor(m_table, m_guard)
        , m_logger(logger)
        , m_misses(misses)
        , m_knobs(std::move(knobs))
        , m_noise(m_knobs.noise)
        , m_sim({}, {0, 0}, Pokemon::Direction::NORTH, false)
        , m_io(m_sim, m_notes, m_misses, m_knobs, m_step, [this]{ return cross(); }, [this](const Pokemon::Pose& far){ cross_to(far); })
        , m_walks(m_io)
        , m_table(story_action_table(m_walks, sim_actions()))
        , m_interrupts(story_interrupts([this](bool fresh){ return look(fresh); }, sim_interrupt_actions()))
        , m_coupons(m_knobs.coupons_at_start)
        , m_poketch(m_knobs.poketch_at_start)
    {
        if (m_knobs.clear_interrupts){
            set_interrupts(&m_interrupts);
        }
    }

    const SimMisses& misses() const{
        return m_misses;
    }
    const SimRun& result() const{
        return m_result;
    }
    //  Every line the walks wrote, the route walker's and the leg records
    //  ("[Leg] ...") among them.
    const std::vector<std::string>& notes() const{
        return m_notes;
    }
    //  The story's actions as the simulation binds them.
    const Pokemon::StoryActionTable& actions() const{
        return m_table;
    }
    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override{
        m_row = data.id;
        m_start_mismatch = data.start ? off_pose(Pokemon::pose_of(*data.start)) : "";
        m_ran.clear();
        m_astray = false;
        m_result.rows.push_back({.row = data.id, .fades = m_misses.fades, .banners = m_misses.banners});
        m_walks.begin_attempt(data.id, data.steps);
        arm("");
    }
    virtual Pokemon::Outcome run(const Pokemon::StoryStep& step, Pokemon::RunState& state) override;

protected:
    //  A dialog that popped up and is still up stops a step that walks or
    //  talks; a step that presses through text closes it.
    virtual Pokemon::Outcome run_action(const Pokemon::StoryStep& step, Pokemon::RunState& state) override;
    virtual Pokemon::Outcome heal(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    virtual Pokemon::Outcome new_game(Pokemon::RunState&, const Pokemon::StoryStep&){ return stub(); }
    virtual Pokemon::Outcome options(Pokemon::RunState&, const Pokemon::StoryStep&){ return stub(); }
    virtual Pokemon::Outcome answer(Pokemon::RunState&, const Pokemon::StoryStep&){ return stub(); }
    virtual Pokemon::Outcome battle(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    virtual Pokemon::Outcome train(Pokemon::RunState&, const Pokemon::StoryStep&){ return stub(); }
    virtual Pokemon::Outcome shop(Pokemon::RunState&, const Pokemon::StoryStep&){ return stub(); }
    virtual Pokemon::Outcome save(Pokemon::RunState&, const Pokemon::StoryStep&){ return stub(); }
    virtual Pokemon::Outcome game(Pokemon::RunState&, const Pokemon::StoryStep&){ return stub(); }

private:
    //  A failed step fails the run as it threw: the simulation has no save
    //  to reload.
    class SimStepGuard : public Pokemon::StepGuard{
    public:
        virtual bool stops_on_failure(const Pokemon::StoryStep&) const override{
            return true;
        }
    };

    //  A step the simulation does not play: its dialogue opened and closed.
    Pokemon::Outcome stub();
    //  A clown's coupon or the president's Poketch, after step `step` passed.
    void count_coupons(const std::string& step);
    StoryBoundActions sim_actions();
    //  The BDSP event `name` on the simulation: the ones it plays, or a stub.
    Pokemon::StoryActionFunction sim_event(const std::string& name);
    //  A wander wait, played from where the step before left us, on its map.
    Pokemon::Outcome wait_for_wanderer(const Pokemon::StoryStep& step, Pokemon::RunState& state);
    //  The screen between steps: the dialog that popped up, if one did; a
    //  battle's opening, its battle on the OPENING_LOOKS-th look. `fresh`
    //  starts a new look.
    Pokemon::ScreenLook look(bool fresh);
    //  What the knobs put on the screen before `step`: a dialog that pops
    //  up, a battle's opening still playing.
    void show_before(const Pokemon::StoryStep& step);
    StoryInterruptActions sim_interrupt_actions();
    const StoryWorld* sim_world(const std::string& map) const;
    SimWorld setup(const StoryWorld* world) const;
    void arm(const std::string& after);
    bool sync(const Pokemon::Pose& pose, const StoryWorld* on = nullptr);
    //  Puts a trigger on walk `step`'s last tiles (SimWorldKnobs::scene_at_walk_end).
    void start_scene_at_end(const Pokemon::StoryStep& step);
    //  Puts the player where SimWorldKnobs::lands says `step` leaves them, if
    //  it says, and counts its coupon or Poketch (count_coupons()).
    void land(const std::string& step);
    bool cross();
    bool cross_to(const Pokemon::Pose& far);
    std::vector<SimWanderer> wanderers(const StoryWorld& world) const;
    std::string off_pose(const Pokemon::Pose& pose) const;
    std::string check(const Pokemon::StoryStep& step, const Pokemon::Outcome& outcome);
    Pokemon::Outcome run_step(const Pokemon::StoryStep& step, Pokemon::RunState& state);
    void place(const StoryWorld* world, const Pokemon::TilePos& start, std::optional<Pokemon::Direction> facing);
    bool settled() const;
    void record_failure(const Pokemon::Outcome& outcome, const Pokemon::StoryStep& step, size_t battles_before);

    Logger& m_logger;
    std::vector<std::string> m_notes;
    SimMisses m_misses;
    SimWorldKnobs m_knobs;
    SimDisturbance* m_noise;
    //  The step running now.
    std::string m_step;
    GridSim m_sim;
    SimStoryStepIO m_io;
    StoryWalkSteps m_walks;
    SimStepGuard m_guard;
    Pokemon::StoryActionTable m_table;
    Pokemon::StoryInterrupts m_interrupts;
    //  The scenes fought so far that SimRun::scenes_fought has.
    size_t m_scenes_fought = 0;
    //  A dialog popped up (SimWorldKnobs::dialog_before) and is still up;
    //  it rose since the last look.
    bool m_dialog_up = false;
    bool m_dialog_rose = false;
    //  A battle's opening is playing before this step
    //  (SimWorldKnobs::battle_opening_before).
    bool m_battle_opening = false;
    //  Looks at the opening so far, from the fresh one.
    size_t m_opening_looks = 0;
    //  The planner's worlds with sim_people() on them, by world and who.
    mutable std::map<std::pair<const StoryWorld*, std::string>, std::unique_ptr<StoryWorld>> m_peopled;
    //  The steps of this row that have run.
    std::set<std::string> m_ran;
    //  The clowns' coupons held and the Poketch (SimWorldKnobs::coupons_at_start).
    std::set<std::string> m_coupons;
    bool m_poketch = true;
    std::string m_row;
    std::vector<detail::RowTrigger> m_armed;
    //  Where the step running now ends, if it says: a door's far side.
    std::optional<Pokemon::Pose> m_far;
    //  Why the player is not on this row's start, or "".
    std::string m_start_mismatch;
    //  Whether the player has been put on a map yet.
    bool m_placed = false;
    //  Whether the step running now is on a room with no map.
    bool m_passed_over = false;
    //  Whether a walk slid on (SimSlide), so the player is not where the
    //  program thinks, until a step's pose_after puts them right.
    bool m_astray = false;
    //  Whether a step left the player off its pose_after (SimWorldKnobs::lands),
    //  so the walks go on from there, checked, until a step's pose_after
    //  puts them right.
    bool m_misplaced = false;
    SimRun m_result;
};

//  Runs `range` of `rows` (the table unless given) on `executor`, stopping
//  at the first failure.
Pokemon::SegmentReport run_rows_on(
    SimExecutor& executor, Logger& logger, const Pokemon::SegmentRange& range,
    const std::vector<StoryCheckpoint>& rows = story_checkpoints()
);
//  Runs `range` of the table on the simulation, stopping at the first failure.
UnitTestResult run_rows(Logger& logger, const Pokemon::SegmentRange& range);
namespace detail{

//  The table with row `row`'s step of `step`'s name replaced by `step`.
std::vector<StoryCheckpoint> table_with(const std::string& row, const Pokemon::StoryStep& step);
//  Whether the run ended on one of `pose`'s tiles.
bool ends_on(const SimRun& run, const Pokemon::Pose& pose);
//  Where the run ended, described; "nowhere" if never placed.
std::string end_of(const SimRun& run);
//  Each row's steps the simulation did not play, a line a row:
//  "01.03: 2 stubbed: save, gym_until_roark_beaten".
std::string stub_report(const SimRun& run);
} // namespace detail



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
