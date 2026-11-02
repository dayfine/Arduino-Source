/*  BDSP AutoStory Story Sim Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows of the story table run as the program runs them (the segment
 *  runner, make_story_segment()), with the program's own walking steps
 *  (StoryWalkSteps: ROUTE, LEGS, STEP, TALK, MASH) on the grid simulation.
 *  After each walking step the simulated player must stand, on the grid,
 *  on a tile the table gives; a step through a door must have warped.
 *
 *  Events, battles, shops and saves are stubs: a dialogue they open is
 *  closed, and the table's pose_after puts the player where they end. A
 *  heal checks that a walk north from where we stand enters a door. A scene
 *  starts when the player steps onto one of the maps' coord events while
 *  its story variable holds (story_vars()). Rooms with no
 *  map (the Trainers' School) are passed over.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Data/PokemonBDSP_AutoStory_CoordEvents.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Utils/PokemonBDSP_AutoStory_GridSim.h"
#include "Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP_AutoStory_StorySim_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::Outcome;
using Pokemon::Pose;
using Pokemon::RunState;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;


//  StoryStepIO on the simulation.
class SimStoryStepIO : public StoryStepIO{
public:
    SimStoryStepIO(GridSim& sim, std::vector<std::string>& notes)
        : m_sim(sim)
        , m_route(sim, notes)
    {}

    virtual RouteIO& route() override{
        return m_route;
    }
    virtual void fight(const std::string&) override{
        m_sim.end_dialogue();
    }
    virtual void walk_until_stopped(Direction direction, size_t tiles, const std::string& label) override{
        m_sim.tap(direction, ROUTE_TURN_TAP, false);
        const Milliseconds limit = ROUTE_START_DELAY + 800ms + route_ms_per_tile(direction) * (int64_t)(2 * tiles);
        const SimMove move = m_sim.hold(direction, limit, SimPace::WALK);
        if (move.stop == SimStop::DIALOG || move.stop == SimStop::WARP){
            m_route.note(label + ": " + sim_stop_name(move.stop) + " instead of a stop.", RouteNote::FAILURE);
        }
    }
    virtual bool run_until_map(Direction direction, Milliseconds limit, const std::string& map, const std::string&) override{
        for (Milliseconds ran = 0ms; ran < limit; ran += ROUTE_RUN_MS_PER_TILE_ACROSS){
            const SimMove move = m_sim.tap(direction, ROUTE_MS_PER_TILE_ACROSS, false);
            if (story_to_local(m_sim.world(), map, nearest_tile(m_sim.player().pos))){
                return true;
            }
            if (move.distance == 0 || move.stop != SimStop::DONE){
                return false;
            }
        }
        return false;
    }
    virtual bool run_into_door(Direction direction, bool run) override{
        return m_sim.hold(direction, 12000ms, run ? SimPace::RUN : SimPace::WALK).stop == SimStop::WARP;
    }
    //  A scene may take us to another map (Mom walks us out, 00.04): with a
    //  dialogue open the stub scene plays it. Otherwise only a door fades.
    virtual void settle(Button, const std::string& label, bool fade) override{
        if (fade && !m_sim.player().warped && !m_sim.player().dialog){
            m_route.note(label + ": no fade, so we are not where the step goes (map mismatch).", RouteNote::FAILURE);
        }
        m_sim.end_dialogue();
    }

private:
    GridSim& m_sim;
    GridSimRouteIO m_route;
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

//  The story variables of 00.04 to 00.05c: Dawn's catching tutorial waits on
//  Route 202 from 00.04's start, Mom calls us back once she has healed us,
//  and the Poketch president waits on Jubilife's row 40 in 00.05b.
std::vector<VarState> story_vars(){
    return {
        {.var = "VAR_ROUTE_202_STATE", .value = 0, .row = "00.04", .after = ""},
        {.var = "CONSOLE_MOMS_CALL", .value = 0, .row = "00.04", .after = "mom_heals_us"},
        {.var = "VAR_POKETCH_CAMPAIGN_STATE", .value = 1, .row = "00.05b", .after = ""},
    };
}

//  The maps' coord events (story_coord_events()) whose variable holds.
std::optional<VarState> var_state(const CoordEvent& event){
    for (const VarState& state : story_vars()){
        if (state.var == event.var && state.value == event.value){
            return state;
        }
    }
    return std::nullopt;
}

RowTrigger row_trigger(const std::string& map, const CoordEvent& event, const VarState& state){
    return {.row = state.row, .after = state.after, .map = map, .trigger = {.name = event.name, .tiles = event.tiles}};
}

std::vector<RowTrigger> story_triggers(){
    std::vector<RowTrigger> triggers;
    for (const auto& [map, json] : bdsp_map_json()){
        for (const CoordEvent& event : story_coord_events(map)){
            const std::optional<VarState> state = var_state(event);
            if (state){
                triggers.push_back(row_trigger(map, event, *state));
            }
        }
    }
    return triggers;
}


//  The program's walking steps on the simulation, the rest stubbed.
class SimExecutor : public Pokemon::StoryStepExecutor{
public:
    explicit SimExecutor(Logger& logger)
        : m_logger(logger)
        , m_sim({}, {0, 0}, Direction::NORTH, true)
        , m_io(m_sim, m_notes)
        , m_walks(m_io)
    {
        //  Mom stands a row above the decomp's (7, 8) from 00.04 on (r43).
        m_house = *story_plan_world("PlayerHouse1F", "PlayerHouse1F");
        m_house.block({7, 7});
    }

    const std::string& error() const{
        return m_error;
    }
    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override{
        m_row = data.id;
        m_walks.begin_attempt(data.id);
        arm("");
    }
    virtual Outcome run(RunState& state, const StoryStep& step) override;

protected:
    virtual Outcome route(RunState& state, const StoryStep& step) override{ return m_walks.route(state, step); }
    virtual Outcome legs(RunState& state, const StoryStep& step) override{ return m_walks.legs(state, step); }
    virtual Outcome step(RunState& state, const StoryStep& step) override{ return m_walks.step(state, step); }
    virtual Outcome talk(RunState& state, const StoryStep& step) override{ return m_walks.talk(state, step); }
    virtual Outcome mash(RunState& state, const StoryStep& step) override{ return m_walks.mash(state, step); }
    virtual Outcome heal(RunState& state, const StoryStep& step) override;
    virtual Outcome new_game(RunState&, const StoryStep&) override{ return stub(); }
    virtual Outcome options(RunState&, const StoryStep&) override{ return stub(); }
    virtual Outcome answer(RunState&, const StoryStep&) override{ return stub(); }
    virtual Outcome battle(RunState&, const StoryStep&) override{ return stub(); }
    virtual Outcome train(RunState&, const StoryStep&) override{ return stub(); }
    virtual Outcome shop(RunState&, const StoryStep&) override{ return stub(); }
    virtual Outcome save(RunState&, const StoryStep&) override{ return stub(); }
    virtual Outcome game(RunState&, const StoryStep&) override{ return stub(); }

private:
    //  A step the simulation does not play: its dialogue opened and closed.
    Outcome stub(){
        m_sim.end_dialogue();
        return Pokemon::outcome_ok();
    }
    const StoryWorld* sim_world(const std::string& map) const;
    SimWorld setup(const StoryWorld* world) const;
    void arm(const std::string& after);
    bool sync(const Pose& pose);
    std::string check(const StoryStep& step, const Outcome& outcome) const;
    Outcome run_step(RunState& state, const StoryStep& step);

    Logger& m_logger;
    std::vector<std::string> m_notes;
    GridSim m_sim;
    SimStoryStepIO m_io;
    StoryWalkSteps m_walks;
    StoryWorld m_house;
    std::string m_row;
    std::vector<RowTrigger> m_armed;
    //  Whether the player has been put on a map yet.
    bool m_placed = false;
    //  Whether the step running now is on a room with no map.
    bool m_passed_over = false;
    std::string m_error;
};

//  The world the player walks on for `map`: the planner's, with what the
//  console showed that the maps lack. Nothing for a room with no map.
const StoryWorld* SimExecutor::sim_world(const std::string& map) const{
    const StoryWorld* world = story_plan_world(map, map);
    if (world == story_plan_world("PlayerHouse1F", "PlayerHouse1F") && m_row >= "00.04"){
        return &m_house;
    }
    return world;
}

SimWorld SimExecutor::setup(const StoryWorld* world) const{
    SimWorld out{.world = world, .trainers = story_trainers(*world)};
    for (const RowTrigger& armed : m_armed){
        if (sim_world(armed.map) != world){
            continue;
        }
        SimTrigger trigger{.name = armed.trigger.name};
        for (const TilePos& pos : armed.trigger.tiles){
            trigger.tiles.push_back(story_to_world(*world, armed.map, pos));
        }
        out.triggers.push_back(std::move(trigger));
    }
    return out;
}

//  Arms the triggers of this row that start after `after`.
void SimExecutor::arm(const std::string& after){
    size_t armed = 0;
    for (const RowTrigger& trigger : story_triggers()){
        if (trigger.row == m_row && trigger.after == after){
            m_armed.push_back(trigger);
            armed++;
        }
    }
    const SimPlayer& player = m_sim.player();
    if (armed > 0 && m_placed && on_grid(player.pos) && !player.dialog){
        m_sim.place(setup(&m_sim.world()), nearest_tile(player.pos), player.facing);
    }
}

//  Puts the player on `pose` unless they stand on one of its tiles already.
//  Returns whether the pose is on a map the simulation has.
bool SimExecutor::sync(const Pose& pose){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(pose);
    const StoryWorld* world = pose.map.empty() ? nullptr : sim_world(pose.map);
    if (world == nullptr || tiles.empty()){
        return false;
    }
    const SimPlayer& player = m_sim.player();
    const std::optional<TilePos> local = !m_placed || &m_sim.world() != world
        ? std::nullopt : story_to_local(*world, pose.map, nearest_tile(player.pos));
    if (local && on_grid(player.pos) && std::find(tiles.begin(), tiles.end(), *local) != tiles.end()){
        return true;
    }
    m_sim.place(setup(world), story_to_world(*world, pose.map, tiles.front()), Pokemon::facing_of(pose));
    m_placed = true;
    return true;
}

Outcome SimExecutor::heal(RunState& state, const StoryStep& step){
    if (sync(state.pose) && m_sim.hold(Direction::NORTH, 3000ms, SimPace::RUN).stop != SimStop::WARP){
        return Pokemon::outcome_failed("no_door", step.name + ": no door north of " + to_string(m_sim.player().pos));
    }
    return Pokemon::outcome_ok();
}

//  Why a walking step did not end where the table says, or "".
std::string SimExecutor::check(const StoryStep& step, const Outcome& outcome) const{
    const bool walks = step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS || step.kind == StoryStepKind::STEP;
    const std::optional<Pose> expected = step.pose_after ? step.pose_after : outcome.pose;
    if (!walks || !expected || Pokemon::tiles_of(*expected).empty() || !m_placed){
        return "";
    }
    const SimPlayer& player = m_sim.player();
    const std::optional<TilePos> local = story_to_local(m_sim.world(), expected->map, nearest_tile(player.pos));
    if (!local){
        return player.warped ? "" : std::format("ended on {} at {}, not on {}.", describe(m_sim.world(), nearest_tile(player.pos)),
            to_string(player.pos), expected->map);
    }
    const std::vector<TilePos> tiles = Pokemon::tiles_of(*expected);
    if (!on_grid(player.pos) || std::find(tiles.begin(), tiles.end(), *local) == tiles.end()){
        return std::format("ended at {} ({}), not on {}.", to_string(player.pos), describe(m_sim.world(), nearest_tile(player.pos)),
            Pokemon::to_string(*expected));
    }
    return "";
}

Outcome SimExecutor::run_step(RunState& state, const StoryStep& step){
    if (story_events().contains(step.variant)){
        return stub();
    }
    //  A scene goes on from where the step before left us.
    const bool walks = step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS ||
        step.kind == StoryStepKind::STEP || step.kind == StoryStepKind::TALK;
    if (walks && !sync(state.pose)){
        m_logger.log(std::format("[{}] {}: no map for {}; passed over.", m_row, step.name, Pokemon::to_string(state.pose)));
        m_passed_over = true;
        return Pokemon::outcome_ok();
    }
    return Pokemon::StoryStepExecutor::run(state, step);
}

Outcome SimExecutor::run(RunState& state, const StoryStep& step){
    Outcome outcome;
    m_passed_over = false;
    try{
        outcome = run_step(state, step);
    }catch (const GridSimFailure& e){
        outcome = Pokemon::outcome_failed("sim_failure", e.message);
    }
    const std::string mismatch = outcome.succeeded() && !m_passed_over ? check(step, outcome) : "";
    if (!mismatch.empty()){
        outcome = Pokemon::outcome_failed("sim_mismatch", step.name + ": " + mismatch);
    }
    m_logger.log(std::format("[{}] {}: {}, the player at {}.", m_row, step.name, Pokemon::to_string(outcome), to_string(m_sim.player().pos)));
    if (!outcome.succeeded() && m_error.empty()){
        m_error = m_row + " " + Pokemon::to_string(outcome);
    }
    arm(step.name);
    return outcome;
}


//  Runs `range` of the table on the simulation, stopping at the first failure.
UnitTestResult run_rows(Logger& logger, const Pokemon::SegmentRange& range){
    SimExecutor executor(logger);
    std::set<std::string> saved;
    Pokemon::Segment segment = make_story_segment(story_checkpoints(), executor, saved);
    Pokemon::SegmentPolicy policy;
    policy.stop_at_first_failure = true;
    Pokemon::SegmentHooks hooks;
    hooks.reload = [](RunState&){ return Pokemon::outcome_ok(); };
    hooks.log = [&logger](const std::string& line){ logger.log(line); };
    RunState state;
    const Pokemon::SegmentReport report = Pokemon::run_segment(state, segment, range, policy, hooks);
    if (!report.completed){
        return std::format("stopped at {} {}: {}", report.failed_checkpoint, report.failed_event, executor.error());
    }
    return true;
}


//  Every coord event has ground the player can step onto, and the ones
//  the story's variables arm are there.
class Test_CoordEvents : public UnitTest{
public:
    Test_CoordEvents()
        : UnitTest("PokemonBDSP::AutoStory - Story sim: the maps' coord events are on open ground or doors")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        for (const auto& [map, json] : bdsp_map_json()){
            const StoryWorld* world = story_plan_world(map, map);
            for (const CoordEvent& event : world == nullptr ? std::vector<CoordEvent>{} : story_coord_events(map)){
                const auto open = [&](const TilePos& pos){
                    const TilePos tile = story_to_world(*world, map, pos);
                    return standable(*world, tile) || world->tile(tile) == Pokemon::TileKind::DOOR;
                };
                //  The decomp's boxes may run into walls (Sandgem's (4, 10..15)).
                if (!std::ranges::any_of(event.tiles, open)){
                    return std::format("{} {}: no tile is open ground or a door.", map, event.name);
                }
            }
        }
        const size_t armed = story_triggers().size();
        return armed == story_vars().size() ? UnitTestResult(true)
            : UnitTestResult(std::format("Expected {} coord events armed by the story's variables, found {}.", story_vars().size(), armed));
    }
};


class Test_StorySim : public UnitTest{
public:
    Test_StorySim(std::string name, std::string from, std::string to)
        : UnitTest("PokemonBDSP::AutoStory - Story sim: " + std::move(name))
        , m_from(std::move(from))
        , m_to(std::move(to))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return run_rows(logger, {.from = m_from, .to = m_to});
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::string m_from;
    std::string m_to;
};

}


void add_tests_AutoStory_StorySim(UnitTestDatabase& database){
    database.add<Test_CoordEvents>();
    database.add<Test_StorySim>("00.04 to 00.04d: home, the catch, Sandgem's Center, Route 202", "00.04", "00.04d");
    database.add<Test_StorySim>("00.05 to 00.05c: Jubilife's Center, School, clowns and Poketch", "00.05", "00.05c");
}



}
}
}
