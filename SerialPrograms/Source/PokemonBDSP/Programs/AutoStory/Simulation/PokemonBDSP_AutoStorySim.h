/*  BDSP AutoStory - Story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story table's rows run on #648's shared story engine
 *  (Pokemon::run_checkpoints()), with the program's own walking steps
 *  (StoryWalkSteps) on the grid simulation; see StorySim_Tests.cpp for
 *  what is simulated and what is stubbed. Moved out of the tests so the
 *  console failure cases (SimFailureCases_Tests) run on the same executor.
 *
// code-health: allow header_symbols -- story simulation executor, scenario runners and mock IO
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStorySim_H
#define PokemonAutomation_PokemonBDSP_AutoStorySim_H

#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStoryTalkOver.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryActions.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryCheckpoint.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryInterrupts.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryStepIO.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryWalkSteps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalkResult.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalkScreen.h"
#include "PokemonBDSP_AutoStoryGridSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Range of checkpoint ids to run in simulation (both inclusive; unset = open end).
struct SimRunRange{
    std::optional<std::string> from = std::nullopt;
    std::optional<std::string> to = std::nullopt;
};

//  Outcome of running a SimRunRange through #648 Pokemon::run_checkpoints().
struct SimRunReport{
    bool completed = false;
    std::string failed_checkpoint = {};
    std::string failed_event = {};
    std::optional<Pokemon::StoryFailure> failure = std::nullopt;
};

//  Which detections the simulation misses on purpose (#162).
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
    bool fading_in = false;
    bool slid = false;
    std::string banner_up = {};
    std::map<std::string, Milliseconds> dialog_waits = {};
};

enum class SimFade{
    BLACK,
    DIM,
    WHITE,
};

struct SimStillTaps{
    std::string step;
    Pokemon::Direction direction = Pokemon::Direction::NORTH;
    std::set<size_t> taps;
};

struct SimSlide{
    std::string step;
    Pokemon::Direction direction = Pokemon::Direction::WEST;
    size_t tiles = 1;
    Pokemon::GridPose far = {};
    SimFade fade = SimFade::DIM;
    bool banner_up = false;
};

namespace detail{
struct SimBeat{
    std::string what;
    bool line = false;
    Milliseconds length = Milliseconds(0);
    bool overworld = true;
    bool moving = false;
    bool at_player = false;
    WarpFade fade = WarpFade::NONE;
};
SimBeat sim_line(std::string what);
SimBeat sim_pause(std::string what, Milliseconds length, bool moving);
SimBeat sim_walk_past(std::string what, Milliseconds length);
SimBeat sim_fade(std::string what, Milliseconds length, WarpFade fade = WarpFade::BLACK);
using SimScene = std::vector<SimBeat>;
std::string play_scene(const SimScene& scene, const Pokemon::SettleRule& rule);
} // namespace detail

struct SimCenterDoors{
    bool late_fade = false;
    Milliseconds fade_in = Milliseconds(0);
    bool stays_inside = false;
};

struct SimWorldKnobs{
    std::vector<SimSlide> slides = {};
    std::map<std::string, double> floor = {};
    std::map<std::string, double> fade_into = {};
    std::optional<uint64_t> wander_seed = std::nullopt;
    std::map<std::string, Milliseconds> first_line = {};
    std::map<std::string, detail::SimScene> scenes = {};
    std::function<bool(const std::string& row, const std::string& step)> dialog_before = nullptr;
    std::function<bool(const std::string& row, const std::string& step)> battle_opening_before = nullptr;
    bool clear_interrupts = true;
    std::map<std::string, Pokemon::GridPose> lands = {};
    SimGrassBattles grass = {};
    std::function<bool(const std::string& row, const std::string& step)> scene_at_walk_end = nullptr;
    bool slow_fade_in = false;
    std::vector<SimStillTaps> still_taps = {};
    std::map<std::string, SimCenterDoors> centers = {};
};

namespace detail{

class SimKnobRouteIO : public GridSimRouteIO{
public:
    SimKnobRouteIO(
        GridSim& sim, std::vector<std::string>& notes, SimMisses& misses, const SimWorldKnobs& knobs,
        const std::string& step, std::function<void(const Pokemon::GridPose&)> cross
    )
        : GridSimRouteIO(sim, notes, {}, {}, knobs.grass)
        , m_grid(sim)
        , m_misses(misses)
        , m_knobs(knobs)
        , m_step(step)
        , m_cross(std::move(cross))
    {}

    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override;
    virtual WalkResult walk_taps(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual bool dialog_within(Milliseconds wait, Color color) override;
    Milliseconds take_uncovered_first_line(){
        return std::exchange(m_uncovered_first_line, Milliseconds(0));
    }
    void check_fade_in(const std::string& label);

private:
    WalkResult slide_on(const SimSlide& slide, WalkResult result);
    WalkResult taps_misread(Pokemon::Direction direction, size_t tiles, const std::string& label, const SimStillTaps& misread);

    GridSim& m_grid;
    SimMisses& m_misses;
    const SimWorldKnobs& m_knobs;
    const std::string& m_step;
    std::function<void(const Pokemon::GridPose&)> m_cross;
    std::set<std::string> m_slid_steps;
    Milliseconds m_uncovered_first_line{0};
};

constexpr size_t SIM_BANNER_TILES = 3;
constexpr Milliseconds SIM_FALSE_DARK = Milliseconds(1800);
constexpr double SIM_FLOOR = 104;
constexpr double SIM_DOOR_FADE = 7;
constexpr double SIM_DIM_FADE = 55;
constexpr double SIM_WHITE_FADE = 250;
constexpr double SIM_FADE_STDDEV = 1;
constexpr double SIM_FLOOR_STDDEV = 20;
ImageStats sim_screen(double brightness, double stddev);
std::vector<Pokemon::TransitionSample> sim_fade_frames(double floor, double fade, bool hold_seen);
WarpFade sim_watch_fade(const WarpFadeRule& rule, double floor, double fade, bool hold_seen);
} // namespace detail

struct SimRun{
    std::string error;
    std::string missed_step;
    std::string failed_step;
    std::vector<std::string> unchecked;
    struct RowStart{
        std::string row;
        size_t fades;
        size_t banners;
        std::vector<std::string> stubbed = {};
    };
    std::vector<RowStart> rows;
    const StoryWorld* world = nullptr;
    Pokemon::TilePos end;
    std::vector<std::string> scenes_fought = {};
};

class SimStoryStepIO : public StoryStepIO{
public:
    SimStoryStepIO(
        GridSim& sim, std::vector<std::string>& notes, SimMisses& misses, const SimWorldKnobs& knobs,
        const std::string& step, std::function<bool()> far_side, std::function<void(const Pokemon::GridPose&)> cross
    )
        : m_sim(sim)
        , m_route(sim, notes, misses, knobs, step, std::move(cross))
        , m_misses(misses)
        , m_knobs(knobs)
        , m_far_side(std::move(far_side))
        , m_step(step)
    {}

    virtual RouteIO& route() override{
        return m_route;
    }
    virtual void fight(const std::string&) override{
        detail::sim_fight(m_sim);
    }
    virtual void walk_until_stopped(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual bool run_until_map(Pokemon::Direction direction, Milliseconds limit, const std::string& map, const std::string&) override;
    virtual WarpSeen run_into_door(Pokemon::Direction direction, bool run, Milliseconds timeout = Milliseconds(12000),
        Milliseconds dark_from = Milliseconds(0), const std::string& banner = "") override{
        m_route.check_fade_in(std::format("{}: run into the door", m_step));
        const SimPace pace = run ? SimPace::RUN : SimPace::WALK;
        if (false_dark() && dark_from <= detail::SIM_FALSE_DARK && detail::SIM_FALSE_DARK < timeout){
            m_sim.hold(direction, detail::SIM_FALSE_DARK, pace);
            return {.fade = WarpFade::BLACK};
        }
        if (m_sim.hold(direction, timeout, pace).stop != SimStop::WARP){
            return {};
        }
        const WarpSeen seen = door_fade(banner);
        m_misses.fading_in = seen.any();
        return seen;
    }
    virtual bool settle(Button, const std::string& label, SettleKind kind, const std::string&) override;
    virtual std::optional<size_t> tap_to_banner(Pokemon::Direction direction, const std::string& map, size_t max_taps) override;

private:
    std::optional<size_t> taps_to_banner(Pokemon::Direction direction, const std::string& map, size_t max_taps);
    enum class FadeShown{
        SEEN,
        HOLD_UNSEEN,
        UNSEEN,
    };
    FadeShown next_fade();
    WarpFade watched_fade(const WarpFadeRule& rule, double fade);
    WarpSeen door_fade(const std::string& banner);
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
    double floor_here() const;

    GridSim& m_sim;
    detail::SimKnobRouteIO m_route;
    SimMisses& m_misses;
    const SimWorldKnobs& m_knobs;
    std::function<bool()> m_far_side;
    const std::string& m_step;
};

namespace detail{

struct RowTrigger{
    std::string row;
    std::string after;
    std::string map;
    SimTrigger trigger;
    std::string var = {};
};

struct VarState{
    std::string var;
    int value;
    std::string row;
    std::string after;
};

std::vector<VarState> story_vars();
std::vector<RowTrigger> story_triggers();
} // namespace detail

//  The program's walking steps on the simulation, compiled into #648
//  Pokemon::StoryCheckpoints and executed by Pokemon::run_checkpoints().
class SimExecutor{
public:
    explicit SimExecutor(Logger& logger, SimMisses misses = {}, SimWorldKnobs knobs = {})
        : m_logger(logger)
        , m_misses(misses)
        , m_knobs(std::move(knobs))
        , m_sim({}, {0, 0}, Pokemon::Direction::NORTH, false)
        , m_io(m_sim, m_notes, m_misses, m_knobs, m_step, [this]{ return cross(); }, [this](const Pokemon::GridPose& far){ cross_to(far); })
        , m_walks(m_io)
        , m_backend([&logger](std::string_view line){ logger.log(std::string(line)); }, m_scope)
        , m_interrupts(story_interrupts([this](bool fresh){ return look(fresh); }, sim_interrupt_actions()))
    {}

    const SimMisses& misses() const{
        return m_misses;
    }
    const SimRun& result() const{
        return m_result;
    }
    const std::vector<std::string>& notes() const{
        return m_notes;
    }

    //  Runs `range` of `rows` through #648 Pokemon::run_checkpoints(),
    //  stopping at the first failure.
    SimRunReport run_range(const SimRunRange& range, const std::vector<StoryCheckpoint>& rows = story_checkpoints());

private:
    std::optional<Pokemon::StoryFailure> heal(BdspRunState& state, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> battle(BdspRunState& state, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> stub();
    void on_attempt_start(const StoryCheckpoint& row);
    std::optional<Pokemon::StoryFailure> run_sim_step(
        const BdspStep& step,
        BdspRunState& state,
        const std::function<std::optional<Pokemon::StoryFailure>()>& run_action
    );
    std::optional<Pokemon::StoryFailure> wrap_step(
        const BdspStep& step,
        BdspRunState& state,
        const std::function<std::optional<Pokemon::StoryFailure>()>& run_action
    );
    void record_step_result(const BdspStep& step, const std::optional<Pokemon::StoryFailure>& failure);
    BdspStoryHandlers sim_handlers();
    Pokemon::ScreenLook look(bool fresh);
    void show_before(const BdspStep& step);
    StoryInterruptActions sim_interrupt_actions();
    const StoryWorld* sim_world(const std::string& map) const;
    SimWorld setup(const StoryWorld* world) const;
    void arm(const std::string& after);
    bool sync(const Pokemon::GridPose& pose, const StoryWorld* on = nullptr);
    void start_scene_at_end(const BdspStep& step);
    void land(const std::string& step);
    bool cross();
    bool cross_to(const Pokemon::GridPose& far);
    std::vector<SimWanderer> wanderers(const StoryWorld& world) const;
    std::string off_pose(const Pokemon::GridPose& pose) const;
    std::string check(const BdspStep& step, const Pokemon::GridPose& after_pose, bool pose_expected);

    Logger& m_logger;
    std::vector<std::string> m_notes;
    SimMisses m_misses;
    SimWorldKnobs m_knobs;
    std::string m_step;
    GridSim m_sim;
    SimStoryStepIO m_io;
    StoryWalkSteps m_walks;
    BdspRunState m_run_state;
    CancellableHolder<CancellableScope> m_scope;
    BdspLogBackend m_backend;
    Pokemon::StoryInterrupts m_interrupts;
    size_t m_scenes_fought = 0;
    bool m_dialog_up = false;
    bool m_dialog_rose = false;
    bool m_battle_opening = false;
    size_t m_opening_looks = 0;
    mutable std::map<std::pair<const StoryWorld*, std::string>, std::unique_ptr<StoryWorld>> m_peopled;
    std::set<std::string> m_ran;
    std::string m_row;
    std::vector<detail::RowTrigger> m_armed;
    std::optional<Pokemon::GridPose> m_far;
    std::string m_start_mismatch;
    bool m_placed = false;
    bool m_passed_over = false;
    bool m_astray = false;
    bool m_misplaced = false;
    SimRun m_result;
};

//  Runs `range` of `rows` (the table unless given) on `executor`, stopping
//  at the first failure.
SimRunReport run_rows_on(
    SimExecutor& executor, Logger& logger, const SimRunRange& range,
    const std::vector<StoryCheckpoint>& rows = story_checkpoints()
);
//  Runs `range` of the table on the simulation, stopping at the first failure.
UnitTestResult run_rows(Logger& logger, const SimRunRange& range);

namespace detail{
//  The table with row `row`'s step of `step`'s name replaced by `step`.
std::vector<StoryCheckpoint> table_with(const std::string& row, const BdspStep& step);
//  Whether the run ended on one of `pose`'s tiles.
bool ends_on(const SimRun& run, const Pokemon::GridPose& pose);
//  Where the run ended, described; "nowhere" if never placed.
std::string end_of(const SimRun& run);
//  Each row's steps the simulation did not play, a line a row.
std::string stub_report(const SimRun& run);
} // namespace detail



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
