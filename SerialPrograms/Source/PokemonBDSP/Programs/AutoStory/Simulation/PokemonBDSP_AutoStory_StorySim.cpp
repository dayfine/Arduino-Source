/*  BDSP AutoStory - Story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StorySim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::Outcome;
using Pokemon::Pose;
using Pokemon::RunState;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;


//  A hold until something stops us. A door stops it at the fade, which
//  fails the step; a fade missed lets the hold go on past the mouth (r81's
//  timed walk south out of the Ravaged Path, onto Route 204's cliff).
void SimStoryStepIO::walk_until_stopped(Direction direction, size_t tiles, const std::string& label){
    m_sim.tap(direction, ROUTE_TURN_TAP, false);
    Milliseconds limit = ROUTE_START_DELAY + 800ms + route_ms_per_tile(direction) * (int64_t)(2 * tiles);
    SimMove move = disturbed_hold(m_sim, m_noise, direction, limit, SimPace::WALK).move;
    if (move.stop == SimStop::WARP && fade_missed() && m_far_side()){
        limit -= std::min(limit, move.time);
        move = disturbed_hold(m_sim, m_noise, direction, limit, SimPace::WALK).move;
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

//  Someone the maps do not have, on `tile` of `map`, talked to or walked round:
//  from row `row` on, until row `until_row`'s step `until` has run if it is set.
struct SimPerson{
    std::string name;
    std::string map;
    TilePos tile;
    std::string row;
    std::string until_row = {};
    std::string until = {};
};
std::vector<SimPerson> sim_people(){
    return {
        //  Mom stands a row above the decomp's (7, 8) from 00.04 on (r43).
        {.name = "mom", .map = "PlayerHouse1F", .tile = {7, 7}, .row = "00.04"},
        //  Barry in the Gym's door, from the welcome (lab-B 2026-11-08) to his lines (r101, r104).
        {.name = "barry", .map = "OreburghCity", .tile = {26, 21}, .row = "01.02b", .until_row = "01.02b", .until = "barrys_lines"},
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

//  The world the player walks on for `map`: the planner's, with what the
//  console showed that the maps lack. Nothing for a room with no map.
const StoryWorld* SimExecutor::sim_world(const std::string& map) const{
    const StoryWorld* world = story_plan_world(map, map);
    std::vector<SimPerson> here;
    std::string who;
    for (const SimPerson& person : sim_people()){
        const bool now = m_row >= person.row && (person.until.empty() || m_row < person.until_row ||
            (m_row == person.until_row && !m_ran.contains(person.until)));
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
    if (m_noise != nullptr){
        out.wanderers = m_noise->wanderers(*world);
    }
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
    if (armed > 0 && m_placed && settled() && !player.dialog){
        place(&m_sim.world(), nearest_tile(player.pos), player.facing);
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
    if (local && settled() && std::find(tiles.begin(), tiles.end(), *local) != tiles.end()){
        return true;
    }
    place(world, story_to_world(*world, pose.map, tiles.front()), Pokemon::facing_of(pose));
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
    place(world, story_to_world(*world, m_far->map, tiles.front()), std::nullopt);
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
        if (settled() && std::ranges::any_of(tiles, there)){
            return "";
        }
    }catch (const InternalProgramError&){}
    return std::format("ended at {} ({}), not on {}.", to_string(player.pos), describe(m_sim.world(), at), Pokemon::to_string(pose));
}

//  The program's run into the Center: past its door, the inside shows, a missed fade or not.
Outcome SimExecutor::heal(RunState& state, const StoryStep& step){
    if (!sync(state.pose)){
        return Pokemon::outcome_ok();
    }
    run_into_center(m_io, m_walks.can_run(), [this]{
        m_misses.fade_unseen = false;
        return m_sim.player().warped.has_value();
    }, [&]{
        throw GridSimFailure{step.name + ": no door north of " + to_string(m_sim.player().pos)};
    });
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

//  Puts the player on `start` of `world`: on the grid, no drift.
void SimExecutor::place(const StoryWorld* world, const TilePos& start, std::optional<Direction> facing){
    m_sim.place(setup(world), start, facing);
    if (m_noise != nullptr){
        m_noise->reset();
    }
}

//  Whether the player stands on their tile: on the grid, or, disturbed,
//  anywhere nearer it than another (how far off is the run's to find out).
bool SimExecutor::settled() const{
    return m_noise != nullptr || on_grid(m_sim.player().pos);
}

void SimExecutor::record_failure(const Outcome& outcome, const StoryStep& step, size_t battles_before){
    //  The walker's last notes, to see why.
    for (size_t i = m_notes.size() - std::min<size_t>(m_notes.size(), 40); i < m_notes.size(); i++){
        m_logger.log(std::format("[{}] note: {}", m_row, m_notes[i]));
    }
    m_result.error = m_row + " " + Pokemon::to_string(outcome);
    m_result.failed_step = m_row + " " + step.name;
    m_result.failure = outcome.failure;
    m_result.failure_detail = outcome.detail;
    m_result.failed_step_battles = m_noise == nullptr ? 0 : m_noise->battles() - battles_before;
}

Outcome SimExecutor::run(RunState& state, const StoryStep& step){
    Outcome outcome;
    m_passed_over = false;
    const size_t battles_before = m_noise == nullptr ? 0 : m_noise->battles();
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
        record_failure(outcome, step, battles_before);
    }
    m_result.battles = m_noise == nullptr ? 0 : m_noise->battles();
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
    const std::vector<StoryCheckpoint>& rows
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



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
