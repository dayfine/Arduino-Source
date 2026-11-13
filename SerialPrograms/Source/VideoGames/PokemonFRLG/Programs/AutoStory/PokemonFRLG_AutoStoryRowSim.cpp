/*  AutoStory Row Sim
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Pokemon_StoryPose.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonFRLG_AutoStoryLegs.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattleDecisions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldGatedRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryHideoutLift.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRoomLegs.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryViridianMart.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSimField.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_FlyMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Outcome;
using Pokemon::RunState;


void SimStubs::begin_row(std::string row){
    m_rows.push_back({std::move(row), {}});
}
void SimStubs::record(const StoryStep& step){
    if (m_rows.empty()){
        m_rows.push_back({"", {}});
    }
    m_rows.back().steps.push_back(step.name);
}
Pokemon::StoryAction SimStubs::stub(Pokemon::StoryActionFunction then){
    return {
        .act = [this, then = std::move(then)](const StoryStep& step, RunState& state){
            record(step);
            return then ? then(step, state) : Pokemon::outcome_ok();
        },
        .needs_console = true,
    };
}

std::string stub_report(const std::vector<RowStubs>& rows){
    std::string out;
    for (const RowStubs& row : rows){
        out += std::format("{}: {} stubbed", row.row, row.steps.size());
        for (size_t i = 0; i < row.steps.size(); i++){
            out += (i == 0 ? ": " : ", ") + row.steps[i];
        }
        out += "\n";
    }
    return out;
}


namespace{

const MapData* map_named(const std::string& name, const std::vector<MapData>& maps = story_maps()){
    for (const MapData& map : maps){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

//  Whether the player, at the end of a WALK `step`, stops there holding on:
//  the map's edge, a wall, or water to Surf onto, but no warp of any kind,
//  even one to a map that isn't loaded.
bool stops_past_end(const RouteStep& step, const std::vector<MapData>& maps){
    const MapData& map = *map_named(step.end.map, maps);
    const TilePos here = step.end.point;
    const TilePos ahead = here.next(step.direction);
    if (!in_bounds(map, ahead)){
        return true;
    }
    if (warps_on_step(map, ahead) || exit_mat(map, here) == step.direction
        || (step.direction == Direction::NORTH && is_door(map, ahead))
    ){
        return false;
    }
    const StepKind kind = classify_step(map, PlayerState{here, elevation_at(map, here), step.surfing}, step.direction);
    return kind == StepKind::BLOCKED || kind == StepKind::START_SURF;
}

//  The first leg of `route` to a wall that holding on would carry past its
//  end, or nothing (console B, 2026-11-02: one rode Route 4's Pokemon
//  Center escalator).
std::optional<std::string> overrun_wall_leg(const std::vector<RouteStep>& route, const std::vector<MapData>& maps){
    for (const RouteStep& step : route){
        if (step.to_wall && !stops_past_end(step, maps)){
            return to_string(step);
        }
    }
    return std::nullopt;
}

//  Where the run stands, with a facing: a wall beside it when unknown.
struct Standing{
    MapPosition position;
    Direction facing = Direction::NORTH;
    std::string error;
};
Standing standing(const Pokemon::Pose& pose, const std::vector<MapData>& maps){
    const std::optional<StoryPose> at = story_pose(pose);
    if (!at){
        return {{}, Direction::NORTH, "no one-tile pose: " + Pokemon::to_string(pose)};
    }
    std::optional<Direction> facing = at->facing;
    if (!facing){
        const MapData* map = map_named(at->map, maps);
        facing = map ? wall_beside(*map, at->tile) : std::nullopt;
    }
    if (!facing){
        return {{}, Direction::NORTH, "facing unknown and no wall beside " + to_string(MapPosition{at->map, at->tile})};
    }
    return {{at->map, at->tile}, *facing, ""};
}

//  An action this simulation plays.
Pokemon::StoryAction played(Pokemon::StoryActionFunction act){
    return {.act = std::move(act), .needs_console = false};
}

} // namespace


StoryMapsSim::StoryMapsSim(StorySimKnobs knobs)
    : Pokemon::TableExecutor(m_table, m_guard)
    , m_table(make_story_action_table([this](StoryActionId id){ return action(id); }))
    , m_knobs(std::move(knobs))
    , m_interrupts(step_interrupts([this](bool){ return look(); }, sim_interrupt_actions()))
{
    if (m_knobs.clear_interrupts){
        set_interrupts(&m_interrupts);
    }
}

void StoryMapsSim::begin_attempt(const Pokemon::StoryCheckpoint& data){
    m_stubs.begin_row(data.id);
    m_row = data.id;
    //  A reload starts the row on the quiet overworld.
    m_dialog_up = m_dialog_rose = false;
}

Outcome StoryMapsSim::run(const StoryStep& step, RunState& state){
    if (m_knobs.dialog_before && m_knobs.dialog_before(m_row, step.name)){
        m_dialog_up = m_dialog_rose = true;
        m_dialog_notes.push_back(m_row + "/" + step.name + ": a dialog pops up before it.");
    }
    return Pokemon::TableExecutor::run(step, state);
}

Outcome StoryMapsSim::run_action(const StoryStep& step, RunState& state){
    if (m_dialog_up){
        if (!presses_through_text(step.kind)){
            return Pokemon::outcome_failed("dialog", step.name + ": a dialog that popped up before it is still up");
        }
        m_dialog_up = false;
    }
    return Pokemon::TableExecutor::run_action(step, state);
}

Pokemon::ScreenLook StoryMapsSim::look(){
    const WallClock start = WallClock() + std::chrono::hours(1);
    Pokemon::ScreenLook out;
    out.log.record({}, start);
    if (std::exchange(m_dialog_rose, false)){
        out.log.record({Pokemon::ScreenEvent::DIALOG_BOX}, start + Milliseconds(33));
    }
    out.window = {.start = start, .end = start + BETWEEN_STEPS_LOOK};
    return out;
}

//  Every kind closes what is up and notes it: the simulation shows only
//  the dialogs it pops up.
StepInterruptActions StoryMapsSim::sim_interrupt_actions(){
    const auto cleared = [this](std::string_view what){
        return [this, what](const Pokemon::InterruptVerdict&, RunState&){
            m_dialog_notes.push_back(std::format("{} cleared before the next step.", what));
            m_dialog_up = false;
            return Pokemon::outcome_ok();
        };
    };
    return StepInterruptActions{
        .wild = cleared("a wild battle"),
        .trainer = cleared("a trainer's battle"),
        .dialog = cleared("a dialog"),
        .warp = cleared("a warp"),
    };
}

Pokemon::StoryAction StoryMapsSim::action(StoryActionId id){
    switch (id){
    case StoryActionId::ROUTE:
        return played([this](const StoryStep& step, RunState& state){ return walk_to(state, step); });
    case StoryActionId::LEGS:
        return played([this](const StoryStep& step, RunState& state){ return legs(state.pose, step); });
    case StoryActionId::STEP:
        return played([this](const StoryStep& step, RunState& state){ return step_ahead(state.pose, step); });
    //  The nurse's way must plan too, but the run goes on from where it stands.
    case StoryActionId::READY:
        return played([this](const StoryStep& step, RunState& state){
            //  The way to the nurse is walked in a copy: the run goes on from here.
            RunState visit = state;
            const Outcome to_nurse = walk_to(visit, step);
            return to_nurse.succeeded() ? Pokemon::outcome_ok() : to_nurse;
        });
    case StoryActionId::SHOP:
        return played([this](const StoryStep& step, RunState&){ return shop(step); });
    //  Pacing leaves the player on either of its tiles, as on the console.
    case StoryActionId::CLOSE_TRAINER_CARD:
    case StoryActionId::LEAVE_GIFT_SUMMARY:
    case StoryActionId::CUT_TREE:
    case StoryActionId::PUSH_BOULDER:
    case StoryActionId::TRASH_CANS:
    case StoryActionId::GAME:
        return m_stubs.stub([](const StoryStep& step, RunState& state){
            return Pokemon::outcome_ok(paced_pose(state.pose, step));
        });
    case StoryActionId::NEW_GAME:
    case StoryActionId::OPTIONS:
    case StoryActionId::TALK:
    case StoryActionId::ANSWER:
    case StoryActionId::MASH:
    case StoryActionId::BATTLE:
    case StoryActionId::HEAL:
        return m_stubs.stub();
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown story action.");
}

Outcome StoryMapsSim::legs(const Pokemon::Pose& pose, const StoryStep& step) const{
    const Standing at = standing(pose, maps());
    if (!at.error.empty()){
        return Pokemon::outcome_failed("no_pose", at.error);
    }
    const std::optional<std::vector<RouteStep>> route = fixed_route(maps(), at.position, Pokemon::story_arg<std::vector<StoryLeg>>(step.args, "legs"));
    if (!route || route->empty()){
        return Pokemon::outcome_failed("bad_legs", to_string(at.position));
    }
    return Pokemon::outcome_ok(to_pose(StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after}));
}

//  A step into a trainer's line goes on until a trainer can see the
//  player, as step_until_seen() does on the console.
Outcome StoryMapsSim::step_ahead(const Pokemon::Pose& pose, const StoryStep& step) const{
    const std::optional<StoryPose> at = story_pose(pose);
    if (!at){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(pose));
    }
    const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
    const MapData* map = map_named(at->map);
    if (step.action != TRAINER_LINE_VARIANT || map == nullptr){
        return Pokemon::outcome_ok(to_pose(StoryPose{at->map, at->tile.next(direction), direction}));
    }
    TilePos onto = at->tile;
    const std::optional<int> stepped = step_until_seen(
        TRAINER_LINE_TILES,
        [&]{ onto = onto.next(direction); },
        [&]{
            return std::any_of(map->objects.begin(), map->objects.end(), [&](const MapObject& object){
                return object.trainer && trainer_may_see(*map, object, onto);
            });
        }
    );
    if (!stepped){
        return Pokemon::outcome_failed("no_trainer", to_string(MapPosition{at->map, onto}));
    }
    return Pokemon::outcome_ok(to_pose(StoryPose{at->map, onto, direction}));
}

//  The Viridian Mart's list as the game shows it. After a purchase the
//  cursor stays on the row bought (console B, 2026-11-01), so each
//  order moves it from there, as buy_items() does since ad5d40885.
Outcome StoryMapsSim::shop(const StoryStep& step){
    const std::vector<Pokemon::StoryItem>& items = Pokemon::story_arg<std::vector<Pokemon::StoryItem>>(step.args, "items");
    static const std::vector<std::string> LIST{"poke_ball", "potion", "antidote", "parlyz_heal", "cancel"};
    const std::optional<std::vector<MartOrder>> orders = viridian_mart_orders(items);
    if (!orders){
        return Pokemon::outcome_failed("unknown_item", step.name);
    }
    int cursor = 0;
    for (size_t c = 0; c < orders->size(); c++){
        const MartOrder& order = (*orders)[c];
        //  The presses: down or up from where the cursor is.
        cursor += order.row - cursor;
        if (cursor < 0 || cursor >= (int)LIST.size() || LIST[cursor] == "cancel"){
            return Pokemon::outcome_failed("mart_cancel", std::format("order {} ends on row {}", c + 1, cursor + 1));
        }
        if (LIST[cursor] != items[c].item){
            return Pokemon::outcome_failed("wrong_item", std::format("{} instead of {}", LIST[cursor], items[c].item));
        }
        m_bag[LIST[cursor]] += order.quantity;
    }
    return Pokemon::outcome_ok();
}

const std::vector<MapData>& StoryMapsSim::maps() const{
    return m_knobs.maps != nullptr ? *m_knobs.maps : story_maps();
}

Outcome StoryMapsSim::walk_to(Pokemon::RunState& state, const StoryStep& step) const{
    const Pokemon::Pose& pose = state.pose;
    const Pokemon::GateRules rules = Pokemon::gate_rules_of(state);
    if (Pokemon::tiles_of(pose).size() > 1){
        //  From the tiles a pacing step leaves: the route must plan, and
        //  test_routes_after_pacing() walks it on the sim from each.
        const std::optional<StoryPose> target = story_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(step.args, "target")));
        const RouteGoal goal{{target->map, target->tile}, target->facing};
        const std::optional<std::vector<RouteStep>> route = plan_story_route(maps(), pose, std::nullopt, goal, state.gates, rules);
        if (!route || route->empty()){
            return Pokemon::outcome_failed("no_route", Pokemon::to_string(pose) + " to " + to_string(goal.position));
        }
        state.gates = gates_after_route(state.gates, *route);
        return Pokemon::outcome_ok(to_pose(StoryPose{target->map, target->tile, route->back().facing_after}));
    }
    const Standing at = standing(pose, maps());
    if (!at.error.empty()){
        return Pokemon::outcome_failed("no_pose", at.error);
    }
    const std::optional<StoryPose> target = story_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(step.args, "target")));
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route_gated(
        maps(), at.position, goal, state.gates, rules, story_route_costs(), at.facing
    );
    if (!route){
        return Pokemon::outcome_failed("no_route", to_string(at.position) + " to " + to_string(goal.position));
    }
    const MapPosition end = follow_overworld_route(maps(), at.position, *route, state.gates);
    if (!(end == goal.position)){
        return Pokemon::outcome_failed("wrong_end", to_string(end));
    }
    if (const std::optional<std::string> leg = overrun_wall_leg(*route, maps())){
        return Pokemon::outcome_failed("wall_overrun", *leg);
    }
    state.gates = gates_after_route(state.gates, *route);
    const Direction facing = route->empty() ? at.facing : route->back().facing_after;
    return Pokemon::outcome_ok(to_pose(StoryPose{target->map, target->tile, facing}));
}


//  The route the program walks for `step`, a ROUTE or LEGS, from `at`:
//  planned, or the legs laid out.
std::optional<std::vector<RouteStep>> walked_route(
    const StoryPose& at, const StoryStep& step,
    const std::vector<MapData>& maps, const Pokemon::GateState& gates, const Pokemon::GateRules& rules
){
    const MapPosition start{at.map, at.tile};
    if (step.kind == StoryStepKind::LEGS){
        return fixed_route(maps, start, Pokemon::story_arg<std::vector<StoryLeg>>(step.args, "legs"));
    }
    const std::optional<StoryPose> target = story_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(step.args, "target")));
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    return plan_overworld_route_gated(maps, start, goal, gates, rules, story_route_costs(), at.facing);
}


OverworldStepSim::OverworldStepSim(OverworldSim& sim, std::string row, StoryPose at)
    : Pokemon::TableExecutor(m_table, m_guard)
    , m_sim(sim)
    , m_at(std::move(at))
    , m_table(make_story_action_table([this](StoryActionId id){ return action(id); }))
{
    m_stubs.begin_row(std::move(row));
}

Pokemon::StoryAction OverworldStepSim::action(StoryActionId id){
    using Body = std::string (OverworldStepSim::*)(const StoryStep&);
    Body body = nullptr;
    switch (id){
    case StoryActionId::ROUTE:
    case StoryActionId::LEGS:
        body = &OverworldStepSim::walk;
        break;
    case StoryActionId::STEP:
        body = &OverworldStepSim::step_ahead;
        break;
    case StoryActionId::CUT_TREE:
        body = &OverworldStepSim::cut;
        break;
    case StoryActionId::PUSH_BOULDER:
        body = &OverworldStepSim::push_boulder;
        break;
    case StoryActionId::BATTLE:
        body = &OverworldStepSim::battle;
        break;
    case StoryActionId::GAME:
        return hideout_or_stub();
    default:
        return m_stubs.stub();
    }
    return played([this, body](const StoryStep& step, RunState&){
        const std::string failure = (this->*body)(step);
        return failure.empty() ? Pokemon::outcome_ok() : Pokemon::outcome_failed("sim_failure", failure);
    });
}

namespace{

//  fight_battle()'s end on the overworld the sim shows after a battle won
//  with a move: the battle loop's polls replayed through its EvolutionHold
//  and BattleEndWatch, as BattleLoop::step() takes them. The overworld of a
//  dark cave reads as the battle's text box with no HP box (console B,
//  2026-11-09, 08.01 hiker_lenny: three 20 s holds for an evolution, then
//  "no battle screen and no START menu for 60 s after the battle"). Empty
//  once START would look for the overworld, else the loop's failure.
std::string battle_end_on_sim(const OverworldSim& sim){
    constexpr Milliseconds POLL(300);
    //  The battle as the loop saw it: a Potion turn's bag, where START
    //  looked for the overworld and found none (as hiker_lenny's turns 2,
    //  4 and 5), then the last turn's text up to the win.
    constexpr Milliseconds BAG(4000);
    constexpr Milliseconds LAST_TEXT(10000);
    //  The loop gives up on the overworld after this (BattleEndWatch's
    //  NO_OVERWORLD); a little longer, so its failure shows.
    constexpr Milliseconds GIVE_UP(65000);
    const WallClock start{};
    Supplies supplies;
    EvolutionHold hold(supplies.lead);
    BattleEndWatch watch;
    watch.seen(BattleSeen::MENU, start);
    Milliseconds at(0);
    for (; at <= BAG; at += POLL){
        watch.seen(BattleSeen::NONE, start + at);
        if (watch.check_overworld(start + at)){
            break;
        }
    }
    hold.turn_taken(TurnChoice::MOVE);
    for (; at <= LAST_TEXT; at += POLL){
        watch.seen(BattleSeen::BATTLE, start + at);
    }
    const bool dark = sim.state().dark;
    const BattleTextScreen overworld{.text_box = dark, .hp_box = false, .arrow = false, .yes_no = false, .dark_cave = dark};
    for (; at <= LAST_TEXT + GIVE_UP; at += POLL){
        if (watch.overworld_lost(start + at)){
            return "fight_battle(): no battle screen and no START menu for 60 s after the battle.";
        }
        if (hold.holds(overworld)){
            hold.held(EvolutionHold::MAX_HOLD, false);
            at += EvolutionHold::MAX_HOLD;
            continue;
        }
        watch.seen(BattleSeen::NONE, start + at);
        if (watch.check_overworld(start + at)){
            return "";
        }
    }
    return "fight_battle(): START never looked for the overworld.";
}

//  wait_out_battle_exit() on `sim`: each look shows the sim's screens for
//  BETWEEN_STEPS_LOOK.
void sim_wait_out_battle_exit(OverworldSim& sim){
    Milliseconds from = sim.state().time;
    const auto look = [&sim, &from](bool fresh){
        if (fresh){
            from = sim.state().time;
        }
        sim.watch({}, BETWEEN_STEPS_LOOK);
        return Pokemon::ScreenLook{
            .log = screen_event_log(sim.state(), from),
            .window = {.start = sim_wall_clock(from), .end = sim_wall_clock(sim.state().time)},
        };
    };
    wait_for_quiet_overworld(look, MovementMechanics::AFTER_BATTLE_QUIET, BATTLE_EXIT_TIMEOUT);
}

} // namespace

//  A ROUTE or LEGS by the sim's walker, fighting a trainer who sees the
//  player on the way.
std::string OverworldStepSim::walk(const StoryStep& step){
    //  The story's maps, as the planner and the program have them (the grunt the sim has
    //  hidden stands on them, and the stairs a switch opens are open); the gates are the sim's,
    //  which the walker's steps open.
    const std::vector<MapData>& maps = story_maps();
    const Pokemon::GateState& gates = m_sim.state().gates;
    const std::optional<std::vector<RouteStep>> route = walked_route(m_at, step, maps, gates, Pokemon::GateRules{.card_key_held = m_sim.state().card_key_held});
    if (route && route->empty()){
        //  Already there.
        return "";
    }
    const RoutePlanning planning{.maps = &maps, .gates = gates, .costs = story_route_costs(), .start = MapPosition{m_at.map, m_at.tile}, .label = step.name};
    SimWalkerRules rules;
    rules.trainer_battle_end = battle_end_on_sim;
    //  A BIKE_VARIANT rides, as on the console, when the sim has the
    //  player on the Bicycle (the rows' sims stub ride_bike): its open
    //  legs are tapped out a tile at a time (counts_bike_taps()). Other
    //  steps keep the sim's walking pace.
    if (move_speed(step) == MoveSpeed::BIKE && m_sim.state().on_bike){
        rules.speed = MoveSpeed::BIKE;
    }
    const std::string failure = route ? OverworldSimWalker(m_sim, rules).run(*route, planning) : "no legs.";
    m_at = route ? StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after} : m_at;
    return failure;
}

//  A hop() or step_onto() (a plain step the table says the end of); else
//  a step one tap at a time, or for a step into a trainer's line by
//  step_until_seen(), each watched for the trainer.
std::string OverworldStepSim::step_ahead(const StoryStep& step){
    if (step.action.empty() && step.pose_after){
        return step_onto(step);
    }
    const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
    if (step.action.empty()){
        if (const std::optional<RouteStep> warp = warp_press(story_maps(), MapPosition{m_at.map, m_at.tile}, direction)){
            return step_onto_warp(*warp);
        }
    }
    const auto tap = [this, direction]{
        sim_step_once(m_sim, direction);
    };
    //  A wild battle, with no message before it, is run from, and the
    //  stepping goes on, as step_into_line() does.
    const auto seen = [this]{
        const std::optional<SimSeen> shown = m_sim.watch(
            {SimWatcher::MESSAGE, SimWatcher::BATTLE_SCREEN}, std::max(TRAINER_LINE_WATCH, WILD_BATTLE_IN_LINE_WATCH)
        );
        if (shown && shown->watcher == 1){
            m_sim.handle_battle(SimBattleChoice::RUN);
            sim_wait_out_battle_exit(m_sim);
            return false;
        }
        return shown.has_value();
    };
    const int tiles = step.action == TRAINER_LINE_VARIANT ? TRAINER_LINE_TILES : 1;
    return step_until_seen(tiles, tap, seen) ? "" : "no trainer saw the player.";
}

//  take_warp_step() on the sim: the step, then its fade, with a step more
//  twice while none shows (wait_out_warp()).
std::string OverworldStepSim::step_onto_warp(const RouteStep& warp){
    const int warps_before = m_sim.state().warps;
    sim_step_once(m_sim, warp.direction);
    for (int tap = 0; tap < 2 && m_sim.state().warps == warps_before; tap++){
        sim_step_once(m_sim, warp.direction);
    }
    if (m_sim.state().warps == warps_before){
        return std::format("no fade in and out after stepping onto the warp to {}.", warp.end.map);
    }
    m_sim.watch({}, OverworldWalkSteps::AFTER_WARP);
    return "";
}

//  A hop() or step_onto(): one step, ending where the table says.
std::string OverworldStepSim::step_onto(const StoryStep& step){
    sim_step_once(m_sim, Pokemon::story_arg<Direction>(step.args, "direction"));
    m_at = *story_pose(Pokemon::pose_of(*step.pose_after));
    return m_sim.state().position == MapPosition{m_at.map, m_at.tile} ? "" : "expected to end on " + to_string(MapPosition{m_at.map, m_at.tile}) + ".";
}

//  A CUT_TREE_VARIANT step: turned to the tree, cut, and a step onto its
//  tile.
std::string OverworldStepSim::cut(const StoryStep& step){
    const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
    if (m_sim.state().facing != direction){
        sim_turn_to(m_sim, direction);
    }
    if (!m_sim.use_field(SimField::CUT_TREE)){
        return "no tree to cut.";
    }
    sim_step_once(m_sim, direction);
    m_at = StoryPose{m_at.map, m_at.tile.next(direction), direction};
    if (!(m_sim.state().position == MapPosition{m_at.map, m_at.tile})){
        return "expected to step onto the tree's tile " + to_string(MapPosition{m_at.map, m_at.tile}) + ".";
    }
    return "";
}

//  A PUSH_BOULDER_VARIANT step: turned to the boulder, and `tiles` pushes,
//  each followed by the step onto the tile the boulder left.
std::string OverworldStepSim::push_boulder(const StoryStep& step){
    const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
    const int64_t tiles = Pokemon::story_arg<int64_t>(step.args, "tiles");
    if (m_sim.state().facing != direction){
        sim_turn_to(m_sim, direction);
    }
    for (int64_t c = 0; c < tiles; c++){
        if (!m_sim.use_field(SimField::PUSH_BOULDER)){
            return "no boulder to push.";
        }
        sim_step_once(m_sim, direction);
        m_at = StoryPose{m_at.map, m_at.tile.next(direction), direction};
        if (!(m_sim.state().position == MapPosition{m_at.map, m_at.tile})){
            return "expected to step onto the boulder's tile " + to_string(MapPosition{m_at.map, m_at.tile}) + ".";
        }
    }
    return "";
}

namespace{

//  walk_room_legs() on the sim: a step tap a tile, each warp's fade and
//  the walk out of a door waited out. A warp taken with no black fade
//  fails, as on the console, unless the leg `dims`.
std::string sim_walk_room_legs(OverworldSim& sim, const std::vector<RoomLeg>& legs){
    for (const RoomLeg& leg : legs){
        if (leg.tiles == 0){
            sim_turn_to(sim, leg.direction);
            continue;
        }
        const int warps_before = sim.state().warps;
        const Milliseconds started = sim.state().time;
        for (int tile = 0; tile < leg.tiles; tile++){
            sim_step_once(sim, leg.direction);
        }
        if (!leg.warps){
            continue;
        }
        const std::vector<SimShown>& shown = sim.state().shown;
        const bool faded = std::any_of(shown.begin(), shown.end(), [&](const SimShown& s){
            return s.screen == SimScreen::FADE && s.at >= started;
        }) || sim.watch({SimWatcher::FADE}, OverworldWalkSteps::AFTER_WARP).has_value();
        if (sim.state().warps != warps_before && !leg.dims && !faded){
            return std::format("walk_overworld_route(): no fade in and out after walking onto the warp to {}.", leg.to);
        }
        sim.watch({}, OverworldWalkSteps::AFTER_WARP);
    }
    return "";
}

} // namespace

namespace{

//  Whether the sim can walk a Pokemon Center's way in or out of `step`:
//  the map it ends on is built into the program. Celadon's Center has no
//  map yet (08.03), so its way in is stubbed.
bool center_walkable(const StoryStep& step){
    if (!is_center_variant(step.action)){
        return false;
    }
    const std::optional<StoryPose> after = step.pose_after ? story_pose(Pokemon::pose_of(*step.pose_after)) : std::nullopt;
    const std::vector<std::pair<std::string, std::string>>& maps = overworld_map_json();
    return after && std::any_of(maps.begin(), maps.end(), [&](const auto& map){ return map.first == after->map; });
}

//  Whether the sim can play a field move: Surf, or Fly to a town whose
//  map is built into the program.
bool field_move_simulated(const StoryStep& step){
    if (parse_surf_step(step)){
        return true;
    }
    const std::optional<FlyPlan> fly = parse_fly_step(step);
    const std::vector<std::pair<std::string, std::string>>& maps = overworld_map_json();
    return fly && std::any_of(maps.begin(), maps.end(), [&](const auto& map){
        return map.first == kanto_fly_towns()[fly->town].landing.map;
    });
}

//  A field move on the sim: turned to the water and Surf, or Fly and its
//  landing waited out, as on the console. The failure, or empty.
std::string sim_field_move(const StoryStep& step, OverworldSim& sim){
    if (const std::optional<Direction> water = parse_surf_step(step)){
        if (sim.state().facing != *water){
            sim_turn_to(sim, *water);
        }
        return sim.use_field(SimField::SURF) ? "" : "no water to Surf onto.";
    }
    const std::optional<FlyPlan> fly = parse_fly_step(step);
    if (!fly || !sim.use_field(SimField::FLY, fly->town)){
        return "no Fly from " + to_string(sim.state().position) + ".";
    }
    sim_wait_out_battle_exit(sim);
    return "";
}

} // namespace

//  A scripted GAME step on the sim: the poster's switch, the lift's ride
//  (its room coded, SimScript::lift_floors, rows as on the panel), a
//  Pokemon Center's way in or out (center_walkable()), or a field move
//  (field_move_simulated()); any other GAME step
//  is stubbed.
Pokemon::StoryAction OverworldStepSim::hideout_or_stub(){
    Pokemon::StoryAction stub = m_stubs.stub();
    Pokemon::StoryAction sim = played([this](const StoryStep& step, RunState&){
        const std::string failure = hideout(step);
        return failure.empty() ? Pokemon::outcome_ok() : Pokemon::outcome_failed("sim_failure", failure);
    });
    sim.act = [stub, played_act = sim.act](const StoryStep& step, RunState& state){
        return is_hideout_step(step) || center_walkable(step) || field_move_simulated(step)
            ? played_act(step, state)
            : stub.act(step, state);
    };
    sim.needs_console = stub.needs_console;
    return sim;
}

std::string OverworldStepSim::hideout(const StoryStep& step){
    if (step.action == POSTER_SWITCH_VARIANT){
        return m_sim.use_field(SimField::POSTER_SWITCH) ? "" : "no poster ahead.";
    }
    if (is_center_variant(step.action)){
        const std::string failure = sim_walk_room_legs(m_sim, center_legs(step.action));
        return failure.empty() ? ended_on(step) : failure;
    }
    if (is_field_move_step(step)){
        const std::string failure = sim_field_move(step, m_sim);
        return failure.empty() ? ended_on(step) : failure;
    }
    const std::optional<LiftRide> ride = parse_lift_floor_step(step);
    if (!ride){
        return "no lift ride \"" + step.action + "\".";
    }
    const std::string in = sim_walk_room_legs(m_sim, lift_in_legs());
    if (!in.empty()){
        return in;
    }
    if (!m_sim.use_field(SimField::LIFT_FLOOR, ride->to)){
        return "no lift panel for \"" + step.action + "\".";
    }
    //  pick_lift_floor()'s wait for the ride.
    sim_wait_out_battle_exit(m_sim);
    const std::string out = sim_walk_room_legs(m_sim, lift_out_legs());
    return out.empty() ? ended_on(step) : out;
}

//  After a scripted step: the player's pose, and whether it is the step's
//  pose_after, if it has one.
std::string OverworldStepSim::ended_on(const StoryStep& step){
    const SimState& state = m_sim.state();
    m_at = StoryPose{state.position.map, state.position.point, state.facing};
    if (!step.pose_after){
        return "";
    }
    const std::optional<StoryPose> expected = story_pose(Pokemon::pose_of(*step.pose_after));
    if (!expected || !(MapPosition{expected->map, expected->tile} == state.position)){
        return "expected to end on " + (expected ? to_string(MapPosition{expected->map, expected->tile}) : std::string("a pose")) + ".";
    }
    return "";
}

//  A battle from the trainer's message, which must be up, ending where the
//  table says, the battle loop's end played on the overworld after it.
std::string OverworldStepSim::battle(const StoryStep& step){
    if (!m_sim.close_message() || m_sim.handle_battle(SimBattleChoice::FIGHT) != SimBattleEnd::ENDED){
        return "no_battle.";
    }
    if (const std::string failure = battle_end_on_sim(m_sim); !failure.empty()){
        return failure;
    }
    //  battle()'s wait_out_battle_exit().
    sim_wait_out_battle_exit(m_sim);
    m_at = *story_pose(Pokemon::pose_of(*step.pose_after));
    if (!(m_sim.state().position == MapPosition{m_at.map, m_at.tile})){
        return "expected " + to_string(MapPosition{m_at.map, m_at.tile}) + ".";
    }
    return "";
}


std::string row_step_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id){
    OverworldStepSim executor(sim, id, at);
    RunState state;
    const Outcome outcome = executor.run(step, state);
    at = executor.at();
    if (outcome.succeeded()){
        return "";
    }
    return id + "/" + step.name + ": " + outcome.detail + " On " + to_string(sim.state().position) + ".";
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
