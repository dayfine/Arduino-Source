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
 *  map (the Trainers' School) are passed over. Each row must start where
 *  the row before left the player.
 *
 *  A tap or hold through a door or cave mouth stops on its tile, as the
 *  console stops at the fade; a hold that does not see the fade goes on
 *  onto the step's pose_after and on along it, and a location banner reads
 *  only a few tiles into its map (SIM_BANNER_TILES). The exit tests run
 *  r75's and r81's old open-ended walks that way.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_CoordEvents.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP_AutoStory_GridSim.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWalkSteps.h"
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

//  Which detections the simulation misses on purpose (#162): the `fade`th
//  door fade (run_into_door(), from 0) or the `banner`th map banner
//  (run_until_map()) of the run goes unseen, though the player did go
//  through. The console missed both (r75, r81).
//  The counts and flags below are the run's: how many fades and banners it
//  looked for, whether a miss happened since the executor last looked, and
//  whether a missed fade is still unseen.
struct SimMisses{
    std::optional<size_t> fade;
    std::optional<size_t> banner;
    size_t fades = 0;
    size_t banners = 0;
    bool missed = false;
    bool fade_unseen = false;
};

//  How many tiles a held walk goes on into a map before its location
//  banner reads: "it shows a tile or a few past the border" (the table's
//  route_201_by_the_path(), measured on the console). Standing still, it
//  reads at once. Through a cave mouth the fade comes first, and a hold
//  whose fade went unseen goes on the same way (r75, r81).
constexpr size_t SIM_BANNER_TILES = 3;

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
    TilePos end;
};

//  StoryStepIO on the simulation. A walk through a door or a cave mouth
//  stops on its tile, as the console stops at the fade; a hold that misses
//  the fade carries on, onto `far_side()` (the step's pose_after: the
//  sim's maps have no warps' destinations) and on along it.
class SimStoryStepIO : public StoryStepIO{
public:
    SimStoryStepIO(GridSim& sim, std::vector<std::string>& notes, SimMisses& misses, std::function<bool()> far_side)
        : m_sim(sim)
        , m_route(sim, notes)
        , m_misses(misses)
        , m_far_side(std::move(far_side))
    {}

    virtual RouteIO& route() override{
        return m_route;
    }
    virtual void fight(const std::string&) override{
        m_sim.end_dialogue();
    }
    virtual void walk_until_stopped(Direction direction, size_t tiles, const std::string& label) override;
    virtual bool run_until_map(Direction direction, Milliseconds limit, const std::string& map, const std::string&) override;
    virtual bool run_into_door(Direction direction, bool run, Milliseconds timeout = Milliseconds(12000)) override{
        const bool warped = m_sim.hold(direction, timeout, run ? SimPace::RUN : SimPace::WALK).stop == SimStop::WARP;
        if (warped && fade_missed()){
            m_misses.fade_unseen = true;
            return false;
        }
        return warped;
    }
    //  A scene may take us to another map (Mom walks us out, 00.04): with a
    //  dialogue open the stub scene plays it. Otherwise only a door fades.
    //  A fade missed on the way in stays missed: it was over before settle()
    //  looked, which then fails as it does on the console after 120 s.
    virtual void settle(Button, const std::string& label, bool fade) override{
        const bool scene_fades = m_sim.player().dialog && m_far_side();
        if (fade && ((!m_sim.player().warped && !scene_fades) || m_misses.fade_unseen)){
            m_route.note(label + ": no fade, so we are not where the step goes (map mismatch).", RouteNote::FAILURE);
        }
        m_misses.fade_unseen = false;
        m_sim.end_dialogue();
    }
    virtual std::optional<size_t> tap_to_banner(Direction direction, const std::string& map, size_t max_taps) override;

private:
    //  Whether the run misses this fade or this banner (SimMisses).
    bool fade_missed(){
        const bool miss = m_misses.fade == m_misses.fades++;
        m_misses.missed |= miss;
        return miss;
    }
    bool banner_missed(){
        const bool miss = m_misses.banner == m_misses.banners++;
        m_misses.missed |= miss;
        return miss;
    }
    bool on_map(const std::string& map) const{
        return story_to_local(m_sim.world(), map, nearest_tile(m_sim.player().pos)).has_value();
    }

    GridSim& m_sim;
    GridSimRouteIO m_route;
    SimMisses& m_misses;
    std::function<bool()> m_far_side;
};

//  A hold until something stops us. A door stops it at the fade, which
//  fails the step; a fade missed lets the hold go on past the mouth (r81's
//  timed walk south out of the Ravaged Path, onto Route 204's cliff).
void SimStoryStepIO::walk_until_stopped(Direction direction, size_t tiles, const std::string& label){
    m_sim.tap(direction, ROUTE_TURN_TAP, false);
    Milliseconds limit = ROUTE_START_DELAY + 800ms + route_ms_per_tile(direction) * (int64_t)(2 * tiles);
    SimMove move = m_sim.hold(direction, limit, SimPace::WALK);
    if (move.stop == SimStop::WARP && fade_missed() && m_far_side()){
        limit -= std::min(limit, move.time);
        move = m_sim.hold(direction, limit, SimPace::WALK);
    }
    if (move.stop == SimStop::DIALOG || move.stop == SimStop::WARP){
        m_route.note(label + ": " + sim_stop_name(move.stop) + " instead of a stop.", RouteNote::FAILURE);
    }
}

//  The D-pad, a tile at a time, until `map`'s banner reads; through a door
//  the walk goes on beyond it (the fade eats no tiles of a hold).
bool SimStoryStepIO::run_until_map(Direction direction, Milliseconds limit, const std::string& map, const std::string&){
    const bool miss = banner_missed();
    size_t on = 0;
    for (Milliseconds ran = 0ms; ran < limit; ran += ROUTE_RUN_MS_PER_TILE_ACROSS){
        SimMove move = m_sim.tap(direction, ROUTE_MS_PER_TILE_ACROSS, false);
        const bool crossed = move.stop == SimStop::WARP && m_far_side();
        const bool stopped = !crossed && (move.distance == 0 || move.stop != SimStop::DONE);
        on = on_map(map) ? on + 1 : 0;
        if (on > 0 && (on > SIM_BANNER_TILES || stopped) && !miss){
            return true;
        }
        if (stopped){
            return false;
        }
    }
    return false;
}

//  D-pad taps, each looked at, until the fade and then the banner (the
//  console's ConsoleStoryStepIO::tap_to_banner()). Each look stands still,
//  so a banner on the far side reads at once. A fade seen ends the taps,
//  banner or not; with the fade missed the next look still sees the banner.
std::optional<size_t> SimStoryStepIO::tap_to_banner(Direction direction, const std::string& map, size_t max_taps){
    bool crossed = false;
    for (size_t tap = 1; tap <= max_taps; tap++){
        const SimMove move = m_sim.tap(direction, 160ms, false);
        if (move.stop == SimStop::DIALOG){
            return std::nullopt;
        }
        bool faded = false;
        if (move.stop == SimStop::WARP){
            faded = !fade_missed();
            crossed = m_far_side();
        }
        if (crossed && on_map(map) && !banner_missed()){
            return tap;
        }
        if (faded){
            return std::nullopt;
        }
    }
    return std::nullopt;
}

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

//  The story variables whose coord events the table walks onto, from the
//  row (and step) the table's reasons give, with the maps' coord events
//  (Diamond's zone events; Platinum's for the houses): Barry's doorstep and
//  Mom's warning in 00.01, Barry's Route 201 warning, then its pull-back
//  row 19 once he has warned us (r14, r16); the lake's entrance in 00.02;
//  Rowan's stop and Dawn at Sandgem's edge in 00.03; Dawn's catching
//  tutorial on Route 202 from 00.04's start, Mom's call once she has healed
//  us; the Poketch president on Jubilife's row 40 in 00.05b; Barry on Route
//  203 in 01.01; the Gate's Hiker and Oreburgh's welcome in 01.02; Barry's
//  bump in Oreburgh after Roark in 01.05; and Rowan in Jubilife in 01.07.
//  Left out: the Twinleaf guitarist (spent upstairs), Platinum's rival
//  steps around our bedroom's start, Dawn at Jubilife's steps (BDSP starts
//  her below them: an event), and Floaroma's (10..12, 29) (Diamond only).
std::vector<VarState> story_vars(){
    return {
        {.var = "VAR_TWINLEAF_TOWN_RIVAL_TRIGGER_STATE", .value = 0, .row = "00.01", .after = ""},
        {.var = "VAR_FOLLOWER_RIVAL_STATE", .value = 0, .row = "00.01", .after = ""},
        {.var = "VAR_FOLLOWER_RIVAL_STATE", .value = 1, .row = "00.01", .after = "barry_warns_us"},
        {.var = "VAR_VERITY_LAKEFRONT_STATE", .value = 0, .row = "00.02", .after = ""},
        {.var = "VAR_FOLLOWER_RIVAL_STATE", .value = 2, .row = "00.03", .after = ""},
        {.var = "VAR_SANDGEM_TOWN_STATE", .value = 0, .row = "00.03", .after = ""},
        {.var = "VAR_ROUTE_202_STATE", .value = 0, .row = "00.04", .after = ""},
        {.var = "CONSOLE_MOMS_CALL", .value = 0, .row = "00.04", .after = "mom_heals_us"},
        {.var = "VAR_POKETCH_CAMPAIGN_STATE", .value = 1, .row = "00.05b", .after = ""},
        {.var = "VAR_ROUTE_203_RIVAL_STATE", .value = 0, .row = "01.01", .after = ""},
        {.var = "VAR_OREBURGH_GATE_1F_HIKER_STATE", .value = 1, .row = "01.02", .after = ""},
        {.var = "VAR_OREBURGH_CITY_STATE", .value = 0, .row = "01.02", .after = ""},
        {.var = "VAR_OREBURGH_CITY_STATE", .value = 2, .row = "01.05", .after = ""},
        {.var = "VAR_JUBILIFE_CITY_STATE", .value = 3, .row = "01.07", .after = ""},
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

//  Someone the maps do not have, who stands on `tile` of `map` and is
//  talked to there or walked round: from row `row` on if `until` is "",
//  else in `row` until its step `until` has run.
struct SimPerson{
    std::string name;
    std::string map;
    TilePos tile;
    std::string row;
    std::string until;
};
std::vector<SimPerson> sim_people(){
    return {
        //  Mom stands a row above the decomp's (7, 8) from 00.04 on (r43).
        {.name = "mom", .map = "PlayerHouse1F", .tile = {7, 7}, .row = "00.04", .until = ""},
        //  Barry stands in the Gym's door until his lines (01.03; Roark
        //  ignored A until then, r101 and r104).
        {.name = "barry", .map = "OreburghCity", .tile = {26, 21}, .row = "01.03", .until = "barrys_lines"},
    };
}

//  A house's stairs, entered by a step `side` from the map's "stairs"
//  point into what the map draws as a wall (the table's 00.01: r8 and r9
//  for ours, r11 for Barry's).
struct SimStairs{
    std::string map;
    Direction side;
};
std::vector<SimStairs> sim_stairs(){
    return {
        {.map = "PlayerHouse2F", .side = Direction::EAST},
        {.map = "RivalHouse1F", .side = Direction::EAST},
        {.map = "RivalHouse2F", .side = Direction::WEST},
    };
}

//  The program's walking steps on the simulation, the rest stubbed.
class SimExecutor : public Pokemon::StoryStepExecutor{
public:
    explicit SimExecutor(Logger& logger, SimMisses misses = {})
        : m_logger(logger)
        , m_misses(misses)
        , m_sim({}, {0, 0}, Direction::NORTH, false)
        , m_io(m_sim, m_notes, m_misses, [this]{ return cross(); })
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
        m_result.rows.push_back({.row = data.id, .fades = m_misses.fades, .banners = m_misses.banners});
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
    bool sync(const Pose& pose, const StoryWorld* on = nullptr);
    bool cross();
    std::string off_pose(const Pose& pose) const;
    std::string check(const StoryStep& step, const Outcome& outcome);
    Outcome run_step(RunState& state, const StoryStep& step);

    Logger& m_logger;
    std::vector<std::string> m_notes;
    SimMisses m_misses;
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
    std::optional<Pose> m_far;
    //  Why the player is not on this row's start, or "".
    std::string m_start_mismatch;
    //  Whether the player has been put on a map yet.
    bool m_placed = false;
    //  Whether the step running now is on a room with no map.
    bool m_passed_over = false;
    SimRun m_result;
};

//  The world the player walks on for `map`: the planner's, with what the
//  console showed that the maps lack. Nothing for a room with no map.
const StoryWorld* SimExecutor::sim_world(const std::string& map) const{
    const StoryWorld* world = story_plan_world(map, map);
    std::vector<SimPerson> here;
    std::string who;
    for (const SimPerson& person : sim_people()){
        const bool now = person.until.empty() ? m_row >= person.row : m_row == person.row && !m_ran.contains(person.until);
        if (world != nullptr && now && story_plan_world(person.map, person.map) == world){
            here.push_back(person);
            who += person.name + ";";
        }
    }
    if (here.empty()){
        return world;
    }
    std::unique_ptr<StoryWorld>& peopled = m_peopled[{world, who}];
    if (!peopled){
        peopled = std::make_unique<StoryWorld>(*world);
        for (const SimPerson& person : here){
            peopled->block(story_to_world(*world, person.map, person.tile));
        }
    }
    return peopled.get();
}

SimWorld SimExecutor::setup(const StoryWorld* world) const{
    //  B runs once the shoes' scene has given them: the scene ends at home.
    SimWorld out{.world = world, .trainers = story_trainers(*world), .can_run = m_walks.can_run()};
    for (const SimStairs& stairs : sim_stairs()){
        if (sim_world(stairs.map) == world){
            out.doors.push_back(story_to_world(*world, stairs.map, map_point(stairs.map, "stairs").next(stairs.side)));
        }
    }
    for (const RowTrigger& armed : m_armed){
        if (sim_world(armed.map) != world){
            continue;
        }
        SimTrigger trigger{.name = armed.trigger.name, .tiles = {}};
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

//  Puts the player on `pose`, on the world `on` if given, unless they stand
//  on one of its tiles already. Returns whether the pose is on a map the
//  simulation has.
bool SimExecutor::sync(const Pose& pose, const StoryWorld* on){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(pose);
    const StoryWorld* world = on != nullptr ? on : pose.map.empty() ? nullptr : sim_world(pose.map);
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

//  Through a door or a cave mouth onto the step's far side, the player's
//  facing kept: a hold goes on from there.
bool SimExecutor::cross(){
    const StoryWorld* world = m_far ? sim_world(m_far->map) : nullptr;
    const std::vector<TilePos> tiles = m_far ? Pokemon::tiles_of(*m_far) : std::vector<TilePos>{};
    if (world == nullptr || tiles.empty()){
        return false;
    }
    m_sim.place(setup(world), story_to_world(*world, m_far->map, tiles.front()), std::nullopt);
    return true;
}

//  Why the player does not stand on one of `pose`'s tiles, or "" if they
//  do or the simulation cannot tell (no map, or not placed yet).
std::string SimExecutor::off_pose(const Pose& pose) const{
    const std::vector<TilePos> tiles = Pokemon::tiles_of(pose);
    if (!m_placed || tiles.empty() || sim_world(pose.map) == nullptr){
        return "";
    }
    const SimPlayer& player = m_sim.player();
    const TilePos at = nearest_tile(player.pos);
    //  In world tiles: a pose a leg worked out from the map it started on
    //  may lie past that map's edge, on the next map (Twinleaf's exit).
    //  story_to_world() throws for a map not on the player's world.
    try{
        const auto there = [&](const TilePos& tile){ return story_to_world(m_sim.world(), pose.map, tile) == at; };
        if (on_grid(player.pos) && std::ranges::any_of(tiles, there)){
            return "";
        }
    }catch (const InternalProgramError&){}
    return std::format("ended at {} ({}), not on {}.", to_string(player.pos), describe(m_sim.world(), at), Pokemon::to_string(pose));
}

Outcome SimExecutor::heal(RunState& state, const StoryStep& step){
    if (sync(state.pose) && m_sim.hold(Direction::NORTH, 3000ms, SimPace::RUN).stop != SimStop::WARP){
        return Pokemon::outcome_failed("no_door", step.name + ": no door north of " + to_string(m_sim.player().pos));
    }
    return Pokemon::outcome_ok();
}

//  Why a walking step did not end where the table says, or "". A step
//  through a door the simulation did not cross is not checked: the sim's
//  maps have no warps' destinations.
std::string SimExecutor::check(const StoryStep& step, const Outcome& outcome){
    const bool walks = step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS || step.kind == StoryStepKind::STEP;
    const std::optional<Pose> expected = step.pose_after ? step.pose_after : outcome.pose;
    if (!walks || !m_placed){
        return "";
    }
    if (!expected || Pokemon::tiles_of(*expected).empty()){
        m_result.unchecked.push_back(m_row + " " + step.name);
        return "";
    }
    const SimPlayer& player = m_sim.player();
    if (player.warped && !story_to_local(m_sim.world(), expected->map, nearest_tile(player.pos))){
        return "";
    }
    return off_pose(*expected);
}

Outcome SimExecutor::run_step(RunState& state, const StoryStep& step){
    if (!m_start_mismatch.empty()){
        return Pokemon::outcome_failed("sim_mismatch", "the row before " + std::exchange(m_start_mismatch, ""));
    }
    m_far = step.pose_after;
    if (story_events().contains(step.variant)){
        return stub();
    }
    //  A scene goes on from where the step before left us. A route walks
    //  on the world it plans on (Jubilife alone has no Route 204 north of it).
    const bool walks = step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS ||
        step.kind == StoryStepKind::STEP || step.kind == StoryStepKind::TALK;
    const StoryWorld* on = step.kind == StoryStepKind::ROUTE && step.target && !state.pose.map.empty()
        ? story_plan_world(state.pose.map, step.target->map) : nullptr;
    //  A world of one map is sim_world()'s, with its people.
    if (on != nullptr && on == story_plan_world(state.pose.map, state.pose.map)){
        on = nullptr;
    }
    if (walks && !sync(state.pose, on)){
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
    //  What the simulation does not play (scenes, events, battles) ends
    //  where the table says it does.
    if (outcome.succeeded() && step.pose_after){
        sync(*step.pose_after);
    }
    m_logger.log(std::format("[{}] {}: {}, the player at {}.", m_row, step.name, Pokemon::to_string(outcome), to_string(m_sim.player().pos)));
    if (std::exchange(m_misses.missed, false) && m_result.missed_step.empty()){
        m_result.missed_step = m_row + " " + step.name;
    }
    if (!outcome.succeeded() && m_result.error.empty()){
        //  The walker's last notes, to see why.
        for (size_t i = m_notes.size() - std::min<size_t>(m_notes.size(), 40); i < m_notes.size(); i++){
            m_logger.log(std::format("[{}] note: {}", m_row, m_notes[i]));
        }
        m_result.error = m_row + " " + Pokemon::to_string(outcome);
        m_result.failed_step = m_row + " " + step.name;
    }
    m_result.world = m_placed ? &m_sim.world() : nullptr;
    m_result.end = nearest_tile(m_sim.player().pos);
    m_ran.insert(step.name);
    arm(step.name);
    return outcome;
}

//  Runs `range` of `rows` (the table unless given) on `executor`, stopping
//  at the first failure.
Pokemon::SegmentReport run_rows_on(
    SimExecutor& executor, Logger& logger, const Pokemon::SegmentRange& range,
    const std::vector<StoryCheckpoint>& rows = story_checkpoints()
){
    std::set<std::string> saved;
    Pokemon::Segment segment = make_story_segment(rows, executor, saved);
    Pokemon::SegmentPolicy policy;
    policy.stop_at_first_failure = true;
    Pokemon::SegmentHooks hooks;
    hooks.reload = [](RunState&){ return Pokemon::outcome_ok(); };
    hooks.log = [&logger](const std::string& line){ logger.log(line); };
    RunState state;
    return Pokemon::run_segment(state, segment, range, policy, hooks);
}

//  Runs `range` of the table on the simulation, stopping at the first failure.
UnitTestResult run_rows(Logger& logger, const Pokemon::SegmentRange& range){
    SimExecutor executor(logger);
    const Pokemon::SegmentReport report = run_rows_on(executor, logger, range);
    for (const std::string& step : executor.result().unchecked){
        logger.log("No end to check after " + step + ".");
    }
    if (!report.completed){
        return std::format("stopped at {} {}: {}", report.failed_checkpoint, report.failed_event, executor.result().error);
    }
    return true;
}

//  The table with row `row`'s step of `step`'s name replaced by `step`.
std::vector<StoryCheckpoint> table_with(const std::string& row, const StoryStep& step){
    std::vector<StoryCheckpoint> rows = story_checkpoints();
    for (StoryCheckpoint& checkpoint : rows){
        for (StoryStep& old : checkpoint.steps){
            if (checkpoint.id == row && old.name == step.name){
                old = step;
                return rows;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No step " + step.name + " in " + row);
}

bool ends_on(const SimRun& run, const Pose& pose){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(pose);
    const std::optional<TilePos> local = run.world ? story_to_local(*run.world, pose.map, run.end) : std::nullopt;
    return local && std::find(tiles.begin(), tiles.end(), *local) != tiles.end();
}
std::string end_of(const SimRun& run){
    return run.world ? describe(*run.world, run.end) : "nowhere";
}

UnitTestResult run_exit_test(Logger& logger, const std::string& row_id, const StoryStep& old_step, const SimMisses& misses, const Pose& overshoot){
    SimExecutor executor(logger, misses);
    const std::vector<StoryCheckpoint> rows = table_with(row_id, old_step);
    run_rows_on(executor, logger, {.from = row_id, .to = row_id}, rows);
    const SimRun& run = executor.result();
    logger.log(std::format("The old {}: {}, ended on {}.", old_step.name, run.error, end_of(run)));
    if (run.failed_step != row_id + " " + old_step.name || !ends_on(run, overshoot)){
        return std::format("the old {} should end on {} and fail there; it ended on {}: {}",
            old_step.name, Pokemon::to_string(overshoot), end_of(run), run.error.empty() ? "no failure" : run.error);
    }
    SimExecutor clean(logger);
    if (!run_rows_on(clean, logger, {.from = row_id, .to = row_id}).completed){
        return "the table's row fails without misses: " + clean.result().error;
    }
    std::vector<SimMisses> runs;
    for (size_t i = 0; i < clean.misses().fades; i++){
        runs.push_back({.fade = i, .banner = std::nullopt});
    }
    for (size_t i = 0; i < clean.misses().banners; i++){
        runs.push_back({.fade = std::nullopt, .banner = i});
    }
    for (const SimMisses& m : runs){
        SimExecutor sim(logger, m);
        run_rows_on(sim, logger, {.from = row_id, .to = row_id});
        if (ends_on(sim.result(), overshoot)){
            return std::format("the table's {} ended on {} with a miss in {}.", old_step.name, end_of(sim.result()),
                sim.result().missed_step);
        }
    }
    return true;
}

//  r75: out of the Oreburgh Gate as a run west until Route 203's banner
//  (the old event west_until_transition()). The banner reads a few tiles
//  in: the run ends west of the Gate's warp tile (55, 9), in the pocket by
//  the pond (Route 203's x 49 to 54, rows 6 to 13).
StoryStep r75_old_gate_exit(){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = "out_of_oreburgh_gate";
    step.legs = {{.direction = Direction::WEST, .tiles = 8}};
    step.variant = "to_map";
    step.pose_after = Pokemon::tile_pose("Route203", {TilePos{55, 9}}, Direction::WEST);
    return step;
}
Pose r75_pond_pocket(){
    std::vector<TilePos> tiles;
    for (int y = 6; y <= 13; y++){
        for (int x = 49; x <= 54; x++){
            tiles.push_back({x, y});
        }
    }
    return Pokemon::tile_pose("Route203", tiles);
}
//  r81: out of the Ravaged Path as a timed walk south (the old event's
//  4 s walk_dir()), its fade missed: on past cave_exit (20, 26) to the
//  cliff, column 20 down to row 30.
StoryStep r81_old_cave_exit(){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = "out_of_the_ravaged_path";
    step.legs = {{.direction = Direction::SOUTH, .tiles = 4}};
    step.variant = "to_wall";
    step.pose_after = Pokemon::tile_pose("Route204North", {TilePos{20, 26}}, Direction::SOUTH);
    return step;
}
Pose r81_below_the_mouth(){
    return Pokemon::tile_pose("Route204North", {TilePos{20, 27}, TilePos{20, 28}, TilePos{20, 29}, TilePos{20, 30}});
}

UnitTestResult test_coord_events(Logger&){
    for (const auto& [map, json] : bdsp_map_json()){
        const StoryWorld* world = story_plan_world(map, map);
        for (const CoordEvent& event : world == nullptr ? std::vector<CoordEvent>{} : story_coord_events(map)){
            const auto open = [&](const TilePos& pos){
                const TilePos tile = story_to_world(*world, map, pos);
                return standable(*world, tile) || world->tile(tile) == Pokemon::TileKind::DOOR;
            };
            if (!std::ranges::any_of(event.tiles, open)){
                return std::format("{} {}: no tile is open ground or a door.", map, event.name);
            }
        }
    }
    const size_t armed = story_triggers().size();
    return armed == story_vars().size() ? UnitTestResult(true)
        : UnitTestResult(std::format("Expected {} coord events armed by the story's variables, found {}.", story_vars().size(), armed));
}

std::string miss(Logger& logger, const SimRun& clean, bool fade, size_t index){
    size_t k = 0;
    while (k + 1 < clean.rows.size() && (fade ? clean.rows[k + 1].fades : clean.rows[k + 1].banners) <= index){
        k++;
    }
    const SimRun::RowStart& start = clean.rows[k];
    SimMisses misses;
    (fade ? misses.fade : misses.banner) = index - (fade ? start.fades : start.banners);
    SimExecutor executor(logger, misses);
    const Pokemon::SegmentReport report = run_rows_on(executor, logger,
        {.from = start.row, .to = clean.rows[std::min(k + 1, clean.rows.size() - 1)].row});
    const SimRun& run = executor.result();
    const std::string what = std::format("{} {}", fade ? "fade" : "banner", index);
    logger.log(std::format("Missed {} in {}: {}.", what, run.missed_step,
        report.completed ? "got over it" : "stopped in " + run.failed_step + ": " + run.error));
    if (report.completed || run.failed_step == run.missed_step){
        return "";
    }
    return std::format("{} missed in {}, failed later in {}: {}", what, run.missed_step, run.failed_step, run.error);
}

UnitTestResult test_story_sim_misses(Logger& logger){
    Pokemon::SegmentRange range{.from = "00.00", .to = "01.11"};
    SimExecutor clean(logger);
    if (!run_rows_on(clean, logger, range).completed){
        return "the run fails without misses: " + clean.result().error;
    }
    logger.log(std::format("{} fades and {} banners to miss.", clean.misses().fades, clean.misses().banners));
    std::vector<std::string> unseen;
    for (size_t i = 0; i < clean.misses().fades + clean.misses().banners; i++){
        const std::string line = miss(logger, clean.result(), i < clean.misses().fades, i < clean.misses().fades ? i : i - clean.misses().fades);
        if (!line.empty()){
            unseen.push_back(line);
        }
    }
    std::string out = "Misses that went unseen:";
    for (const std::string& line : unseen){
        out += "\n  " + line;
    }
    return unseen.empty() ? UnitTestResult(true) : UnitTestResult(out);
}

UnitTestResult test_story_sim_whole_table(Logger& logger){
    return run_rows(logger, {.from = "00.00", .to = "01.11"});
}

UnitTestResult test_story_sim_each_row(Logger& logger){
    for (const StoryCheckpoint& row : story_checkpoints()){
        const auto res = run_rows(logger, {.from = row.id, .to = row.id});
        if (res.result != UnitTestResult::PASSED){
            return std::format("Row {} failed: {}", row.id, res.message);
        }
    }
    return true;
}

UnitTestResult test_exit_gate(Logger& logger){
    return run_exit_test(logger, "01.05", r75_old_gate_exit(), SimMisses{}, r75_pond_pocket());
}

UnitTestResult test_exit_ravaged_path(Logger& logger){
    return run_exit_test(logger, "01.09", r81_old_cave_exit(), SimMisses{.fade = 0, .banner = std::nullopt}, r81_below_the_mouth());
}

//  Oreburgh Mine B1F: the entrance hall corridor is traversed to and back by
//  running south from (12, 1) to (12, 21), and north from (11, 21) to (11, 1).
UnitTestResult test_oreburgh_mine_hall_corridor(Logger& logger){
    const StoryWorld* world = story_plan_world("OreburghMineB1F", "OreburghMineB1F");
    if (world == nullptr){
        return "OreburghMineB1F world not found.";
    }
    const SimWorld sim_world{.world = world, .can_run = true};
    const auto test_run = [&](TilePos start, Direction dir, TilePos expected, const char* label){
        GridSim sim(sim_world, start, dir, true);
        const SimMove move = sim.hold(dir, 5000ms, SimPace::RUN);
        if (move.stop != SimStop::WARP || sim.player().pos != sim_pos(expected)){
            return std::format("{}: expected WARP at {}, got {} at {}.", label,
                Pokemon::to_string(expected), sim_stop_name(move.stop), Pokemon::to_string(nearest_tile(sim.player().pos)));
        }
        logger.log(std::format("{} hall run reached {} in {} ms.", label, Pokemon::to_string(expected), move.time.count()));
        return std::string{};
    };
    std::string err = test_run({12, 1}, Direction::SOUTH, {12, 21}, "Southbound");
    if (!err.empty()){
        return err;
    }
    err = test_run({11, 21}, Direction::NORTH, {11, 1}, "Northbound");
    return err.empty() ? UnitTestResult(true) : UnitTestResult(err);
}

//  Oreburgh Mine B1F: tapping 19 tiles leaves the player 1 tile short of the warp door.
StoryStep old_mine_hall_19(std::string name, Direction direction){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = std::move(name);
    step.legs = {{.direction = direction, .tiles = 19}};
    return step;
}

//  Oreburgh Mine: stepping south at col 29 without bypassing the crate
//  hits the wooden crate at (29, 28) on the south leg.
StoryStep old_coal_room_crate_cut(){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = "below_roark";
    step.legs = {
        {.direction = Direction::EAST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 2},
        {.direction = Direction::WEST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 5},
        {.direction = Direction::EAST, .tiles = 14}, {.direction = Direction::SOUTH, .tiles = 10},
        {.direction = Direction::SOUTH, .tiles = 2}, {.direction = Direction::WEST, .tiles = 11},
    };
    return step;
}

UnitTestResult test_story_sim_step_failure(Logger& logger, const std::string& row, const StoryStep& old_step, const std::string& expected_failed_step, const std::string& expected_error){
    SimExecutor executor(logger);
    const std::vector<StoryCheckpoint> rows = table_with(row, old_step);
    run_rows_on(executor, logger, {.from = row, .to = row}, rows);
    const SimRun& run = executor.result();
    if (run.failed_step != expected_failed_step || run.error.find(expected_error) == std::string::npos){
        return std::format("expected old {} to fail at '{}' with '{}', but got failed_step='{}', error='{}'.",
            old_step.name, expected_failed_step, expected_error, run.failed_step, run.error);
    }
    return true;
}

struct AutoStoryStorySimInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryStorySimExpectation{
};

struct AutoStoryStorySimCase{
    AutoStoryStorySimInput input;
    AutoStoryStorySimExpectation expectation;
};

class Test_AutoStoryStorySim : public UnitTest{
public:
    explicit Test_AutoStoryStorySim(AutoStoryStorySimCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Story sim: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryStorySimCase m_case;
};

} // namespace

const AutoStoryStorySimCase AUTO_STORY_STORY_SIM_CASES[] = {
    {
        .input = {"the maps' coord events are on open ground or doors", test_coord_events},
        .expectation = {},
    },
    {
        .input = {"detector misses from 00.00 to 01.11", test_story_sim_misses},
        .expectation = {},
    },
    {
        .input = {"00.00 to 01.11: the whole table, from the new game to Floaroma", test_story_sim_whole_table},
        .expectation = {},
    },
    {
        .input = {"each row from 00.00 to 01.11 in isolation", test_story_sim_each_row},
        .expectation = {},
    },
    {
        .input = {"r75, out of the Oreburgh Gate", test_exit_gate},
        .expectation = {},
    },
    {
        .input = {"r81, out of the Ravaged Path", test_exit_ravaged_path},
        .expectation = {},
    },
    {
        .input = {"01.03, Oreburgh Mine B1F entrance hall corridor runs to and back", test_oreburgh_mine_hall_corridor},
        .expectation = {},
    },
    {
        .input = {"01.03, counted 19 taps south leaves player in hall", [](Logger& logger){
            return test_story_sim_step_failure(logger, "01.03", old_mine_hall_19("into_the_coal_room", Direction::SOUTH), "01.03 south_to_the_rock", "warp instead of a stop.");
        }},
        .expectation = {},
    },
    {
        .input = {"01.03, counted 19 taps north leaves player in hall", [](Logger& logger){
            return test_story_sim_step_failure(logger, "01.03", old_mine_hall_19("out_of_the_mine", Direction::NORTH), "01.03 up_to_the_mines_sign", "warp instead of a stop.");
        }},
        .expectation = {},
    },
    {
        .input = {"01.03, cut corner through coal room crate at col 29", [](Logger& logger){
            return test_story_sim_step_failure(logger, "01.03", old_coal_room_crate_cut(), "01.03 below_roark", "short_leg: south 2 moved 0");
        }},
        .expectation = {},
    },
};

void add_tests_AutoStory_StorySim(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_STORY_SIM_CASES){
        database.add<Test_AutoStoryStorySim>(item);
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
