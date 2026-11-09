/*  BDSP AutoStory - Story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story table's rows run as the program runs them (the segment
 *  runner, make_story_segment()), with the program's own walking steps
 *  (StoryWalkSteps) on the grid simulation; see StorySim_Tests.cpp for
 *  what is simulated and what is stubbed. Moved out of the tests so the
 *  console failure cases (SimFailureCases_Tests) run on the same executor.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StorySim_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StorySim_H

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
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP_AutoStory_GridSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Which detections the simulation misses on purpose (#162): the `fade`th
//  door fade (run_into_door(), from 0) or the `banner`th map banner
//  (run_until_map()) of the run goes unseen, though the player did go
//  through. The console missed both (r75, r81). The `dark`th door run reads
//  dark SIM_FALSE_DARK in, short of the door, as lab-B's did on 2026-11-08.
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
    std::optional<size_t> dark = {};
    std::optional<size_t> through = {};
    bool through_banner = false;
    size_t tap_steps = 0;
    size_t fades = 0;
    size_t banners = 0;
    size_t doors = 0;
    bool missed = false;
    bool fade_unseen = false;
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
    //  How long after the A press a talk's first line shows, by step name.
    std::map<std::string, Milliseconds> first_line = {};
};

//  The walker's RouteIO on the story simulation, with SimWorldKnobs' slides
//  and late first lines. `step` names the step running now; `cross` puts
//  the player on a pose past a warp.
class SimKnobRouteIO : public GridSimRouteIO{
public:
    SimKnobRouteIO(
        GridSim& sim, std::vector<std::string>& notes, SimMisses& misses, const SimWorldKnobs& knobs,
        const std::string& step, std::function<void(const Pokemon::Pose&)> cross
    )
        : GridSimRouteIO(sim, notes)
        , m_grid(sim)
        , m_misses(misses)
        , m_knobs(knobs)
        , m_step(step)
        , m_cross(std::move(cross))
    {}

    virtual WalkResult walk_taps(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual bool dialog_within(Milliseconds wait, Color color) override;

private:
    WalkResult slide_on(const SimSlide& slide, WalkResult result);

    GridSim& m_grid;
    SimMisses& m_misses;
    const SimWorldKnobs& m_knobs;
    const std::string& m_step;
    std::function<void(const Pokemon::Pose&)> m_cross;
    std::set<std::string> m_slid_steps;
};

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
    //  Each row run, with the fades and banners looked for before it.
    struct RowStart{
        std::string row;
        size_t fades;
        size_t banners;
    };
    std::vector<RowStart> rows;
    //  Where the player stood after the last step, if placed.
    const StoryWorld* world = nullptr;
    Pokemon::TilePos end;
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
        , m_route(sim, notes, misses, knobs, step, std::move(cross))
        , m_misses(misses)
        , m_knobs(knobs)
        , m_far_side(std::move(far_side))
    {}

    virtual RouteIO& route() override{
        return m_route;
    }
    virtual void fight(const std::string&) override{
        m_sim.end_dialogue();
    }
    virtual void walk_until_stopped(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual bool run_until_map(Pokemon::Direction direction, Milliseconds limit, const std::string& map, const std::string&) override;
    //  A door's fade stops the run; with the fade missed, the run goes on
    //  holding and sees the far map's `banner` show after it, if it has one
    //  (story_arrival_banner(): the sim's maps have no banners of their own).
    virtual WarpSeen run_into_door(Pokemon::Direction direction, bool run, Milliseconds timeout = Milliseconds(12000),
        Milliseconds dark_from = Milliseconds(0), const std::string& banner = "") override{
        const SimPace pace = run ? SimPace::RUN : SimPace::WALK;
        if (false_dark() && dark_from <= SIM_FALSE_DARK && SIM_FALSE_DARK < timeout){
            m_sim.hold(direction, SIM_FALSE_DARK, pace);
            return {.fade = WarpFade::BLACK};
        }
        if (m_sim.hold(direction, timeout, pace).stop != SimStop::WARP){
            return {};
        }
        if (!fade_missed()){
            return {.fade = WarpFade::BLACK};
        }
        m_misses.fade_unseen = true;
        return {.banner = !banner.empty() && !banner_missed() ? banner : ""};
    }
    //  A scene may take us to another map (Mom walks us out, 00.04): with a
    //  dialogue open the stub scene plays it. Otherwise only a door fades.
    //  A fade missed on the way in stays missed: it was over before settle()
    //  looked, which then fails as it does on the console after 120 s. The
    //  run before looked for the banner (run_into_door()), so settle() sees
    //  none here.
    virtual void settle(Button, const std::string& label, bool fade, const std::string&) override{
        const bool scene_fades = m_sim.player().dialog && m_far_side();
        if (fade && ((!m_sim.player().warped && !scene_fades) || m_misses.fade_unseen)){
            m_route.note(label + ": no fade, so we are not where the step goes (map mismatch).", RouteNote::FAILURE);
        }
        m_misses.fade_unseen = false;
        m_sim.end_dialogue();
    }
    virtual std::optional<size_t> tap_to_banner(Pokemon::Direction direction, const std::string& map, size_t max_taps) override;

private:
    //  Whether the run misses this fade or this banner (SimMisses).
    bool fade_missed(){
        const bool miss = m_misses.fade == m_misses.fades++;
        m_misses.missed |= miss;
        return miss;
    }
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
    SimKnobRouteIO m_route;
    SimMisses& m_misses;
    const SimWorldKnobs& m_knobs;
    std::function<bool()> m_far_side;
};

//  A scene's trigger the simulation fires from row `row` on, once the step
//  `after` has run ("" from the row's start), on `map`'s `tiles`.
struct RowTrigger{
    std::string row;
    std::string after;
    std::string map;
    SimTrigger trigger;
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

//  The program's walking steps on the simulation, the rest stubbed.
class SimExecutor : public Pokemon::StoryStepExecutor{
public:
    explicit SimExecutor(Logger& logger, SimMisses misses = {}, SimWorldKnobs knobs = {})
        : m_logger(logger)
        , m_misses(misses)
        , m_knobs(std::move(knobs))
        , m_sim({}, {0, 0}, Pokemon::Direction::NORTH, false)
        , m_io(m_sim, m_notes, m_misses, m_knobs, m_step, [this]{ return cross(); }, [this](const Pokemon::Pose& far){ cross_to(far); })
        , m_walks(m_io)
    {}

    const SimMisses& misses() const{
        return m_misses;
    }
    const SimRun& result() const{
        return m_result;
    }
    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override{
        m_row = data.id;
        m_start_mismatch = data.start ? off_pose(*data.start) : "";
        m_ran.clear();
        m_astray = false;
        m_result.rows.push_back({.row = data.id, .fades = m_misses.fades, .banners = m_misses.banners});
        m_walks.begin_attempt(data.id);
        arm("");
    }
    virtual Pokemon::Outcome run(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;

protected:
    virtual Pokemon::Outcome route(Pokemon::RunState& state, const Pokemon::StoryStep& step) override{ return m_walks.route(state, step); }
    virtual Pokemon::Outcome legs(Pokemon::RunState& state, const Pokemon::StoryStep& step) override{ return m_walks.legs(state, step); }
    virtual Pokemon::Outcome step(Pokemon::RunState& state, const Pokemon::StoryStep& step) override{ return m_walks.step(state, step); }
    virtual Pokemon::Outcome talk(Pokemon::RunState& state, const Pokemon::StoryStep& step) override{ return m_walks.talk(state, step); }
    virtual Pokemon::Outcome mash(Pokemon::RunState& state, const Pokemon::StoryStep& step) override{ return m_walks.mash(state, step); }
    virtual Pokemon::Outcome heal(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome new_game(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome options(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome answer(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome battle(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome train(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome shop(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome save(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome game(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }

private:
    //  A step the simulation does not play: its dialogue opened and closed.
    Pokemon::Outcome stub(){
        m_sim.end_dialogue();
        return Pokemon::outcome_ok();
    }
    const StoryWorld* sim_world(const std::string& map) const;
    SimWorld setup(const StoryWorld* world) const;
    void arm(const std::string& after);
    bool sync(const Pokemon::Pose& pose, const StoryWorld* on = nullptr);
    bool cross();
    bool cross_to(const Pokemon::Pose& far);
    std::vector<SimWanderer> wanderers(const StoryWorld& world) const;
    std::string off_pose(const Pokemon::Pose& pose) const;
    std::string check(const Pokemon::StoryStep& step, const Pokemon::Outcome& outcome);
    Pokemon::Outcome run_step(Pokemon::RunState& state, const Pokemon::StoryStep& step);

    Logger& m_logger;
    std::vector<std::string> m_notes;
    SimMisses m_misses;
    SimWorldKnobs m_knobs;
    //  The step running now.
    std::string m_step;
    GridSim m_sim;
    SimStoryStepIO m_io;
    StoryWalkSteps m_walks;
    //  The planner's worlds with sim_people() on them, by world and who.
    mutable std::map<std::pair<const StoryWorld*, std::string>, std::unique_ptr<StoryWorld>> m_peopled;
    //  The steps of this row that have run.
    std::set<std::string> m_ran;
    std::string m_row;
    std::vector<RowTrigger> m_armed;
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
//  The table with row `row`'s step of `step`'s name replaced by `step`.
std::vector<StoryCheckpoint> table_with(const std::string& row, const Pokemon::StoryStep& step);
//  Whether the run ended on one of `pose`'s tiles.
bool ends_on(const SimRun& run, const Pokemon::Pose& pose);
//  Where the run ended, described; "nowhere" if never placed.
std::string end_of(const SimRun& run);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
