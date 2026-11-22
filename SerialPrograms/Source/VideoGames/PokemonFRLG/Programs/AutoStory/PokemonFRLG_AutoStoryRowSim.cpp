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
#include "Pokemon/Navigation/Grid/Maps/Pokemon_GateState.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_FlyMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldGatedRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSimField.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattleDecisions.h"
#include "PokemonFRLG_AutoStoryBoulders.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryHideoutLift.h"
#include "PokemonFRLG_AutoStoryLegs.h"
#include "PokemonFRLG_AutoStoryLineTrainer.h"
#include "PokemonFRLG_AutoStoryMovedBoulders.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryRoomLegs.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryViridianMart.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::StoryFailure;


void SimStubs::begin_row(std::string row){
    m_rows.push_back({std::move(row), {}});
}

void SimStubs::record(const StoryStep& step){
    if (m_rows.empty()){
        m_rows.push_back({"", {}});
    }
    m_rows.back().steps.push_back(step.name);
}

FrlgStepAction SimStubs::stub(FrlgStepAction then){
    return [this, then = std::move(then)](const StoryStep& step, FrlgRunState& state) -> std::optional<StoryFailure>{
        record(step);
        return then ? then(step, state) : std::nullopt;
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

bool stops_past_end(const RouteStep& step, const std::vector<MapData>& maps){
    const MapData& map = *map_named(step.end.map, maps);
    const TilePos here = step.end.point;
    const TilePos ahead = here.next(step.direction);
    if (!in_bounds(map, ahead)){
        return !std::ranges::any_of(map.connections, [&](const MapConnection& connection){
            return connection.side == step.direction;
        });
    }
    if (warps_on_step(map, ahead) || exit_mat(map, here) == step.direction
        || (step.direction == Direction::NORTH && is_door(map, ahead))
    ){
        return false;
    }
    const StepKind kind = classify_step(map, PlayerState{here, elevation_at(map, here), step.surfing}, step.direction);
    return kind == StepKind::BLOCKED || kind == StepKind::START_SURF;
}

std::optional<std::string> overrun_wall_leg(const std::vector<RouteStep>& route, const std::vector<MapData>& maps){
    for (const RouteStep& step : route){
        if (step.to_wall && !stops_past_end(step, maps)){
            return to_string(step);
        }
    }
    return std::nullopt;
}

struct Standing{
    MapPosition position = {};
    Direction facing = Direction::NORTH;
    std::string error = {};
};

Standing standing(const std::optional<Pokemon::GridPose>& pose, const std::vector<MapData>& maps){
    const std::optional<StoryPose> at = story_pose(pose);
    if (!at){
        return {{}, Direction::NORTH, "no one-tile pose: " + format_grid_pose(pose)};
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

} // namespace


StoryMapsSim::StoryMapsSim(StorySimKnobs knobs)
    : m_table(make_story_action_table([this](StoryActionId id){ return action(id); }))
    , m_knobs(std::move(knobs))
    , m_interrupts(step_interrupts([this](bool){ return look(); }, sim_interrupt_actions()))
{}

void StoryMapsSim::begin_attempt(const StoryCheckpoint& data){
    m_stubs.begin_row(data.id);
    m_row = data.id;
    m_dialog_up = m_dialog_rose = false;
}

namespace{

void apply_step_post_state(const StoryStep& step, FrlgRunState& state){
    if (step.pose_after){
        state.pose = step.pose_after;
    }
    if (step.gates_after){
        Pokemon::apply_gate_change(state.gates, *step.gates_after);
    }
    for (const std::string& flag : step.flags_after){
        state.flags.insert(flag);
    }
}

} // namespace

std::optional<StoryFailure> StoryMapsSim::run_step(const StoryStep& step, FrlgRunState& state){
    if (!step.skip_if.empty() && state.flags.contains(step.skip_if)){
        return std::nullopt;
    }
    if (step.check && !Pokemon::runs_check(m_knobs.checks, *step.check)){
        return std::nullopt;
    }
    if (m_knobs.dialog_before && m_knobs.dialog_before(m_row, step.name)){
        m_dialog_up = m_dialog_rose = true;
        m_dialog_notes.push_back(m_row + "/" + step.name + ": a dialog pops up before it.");
    }
    if (m_knobs.clear_interrupts){
        const Pokemon::InterruptCheck interrupt = Pokemon::clear_story_interrupt(m_interrupts, step.name, step.action);
        if (interrupt.pose_lost){
            state.pose = std::nullopt;
        }
        if (interrupt.failure){
            return interrupt.failure;
        }
    }
    if (m_dialog_up){
        if (!presses_through_text(step.action)){
            return frlg_failure("dialog", step.name + ": a dialog that popped up before it is still up");
        }
        m_dialog_up = false;
    }
    const auto it = m_table.find(step.action);
    if (it == m_table.end()){
        return frlg_unsupported_step(step);
    }
    std::optional<StoryFailure> failure = trainers_back_on_warp(
        [&]{ return regrow_on_warp(maps(state), [&]{ return it->second(step, state); }, state); },
        state
    );
    if (failure){
        return failure;
    }
    apply_step_post_state(step, state);
    return std::nullopt;
}

Pokemon::StoryCheckpoint StoryMapsSim::make_checkpoint(
    const StoryCheckpoint& data, FrlgRunState& state,
    SavePoseReader start_pose_reader, StoryLog start_log
){
    Pokemon::StoryCheckpoint cp;
    cp.id = data.id;
    cp.title = data.title;

    const auto init = [this, &data, &state](){
        begin_attempt(data);
        state.pose = data.start;
        state.gates = data.start_gates;
        state.flags.insert(data.start_flags.begin(), data.start_flags.end());
    };

    const bool has_start_check = static_cast<bool>(start_pose_reader) && checked_start_pose(data).has_value();
    if (has_start_check){
        Pokemon::StoryStep check_step;
        check_step.name = "save_at_start_pose";
        check_step.action = "start_check";
        check_step.run = [&data, init, read = std::move(start_pose_reader), log = std::move(start_log)](
            Pokemon::StoryBackend&, Pokemon::RunState&
        ) -> std::optional<StoryFailure>{
            init();
            return run_start_pose_check(checked_start_pose(data), read, log);
        };
        cp.steps.push_back(std::move(check_step));
    }

    for (size_t i = 0; i < data.steps.size(); i++){
        const StoryStep& step = data.steps[i];
        const bool needs_init = !has_start_check && i == 0;
        Pokemon::StoryStep engine_step;
        engine_step.name = step.name;
        engine_step.why = step.why;
        engine_step.action = step.action;
        engine_step.run = [this, step, &state, needs_init, init](
            Pokemon::StoryBackend&, Pokemon::RunState&
        ) -> std::optional<StoryFailure>{
            if (needs_init){
                init();
            }
            return run_step(step, state);
        };
        cp.steps.push_back(std::move(engine_step));
    }
    return cp;
}

Pokemon::ScreenLook StoryMapsSim::look(){
    const WallClock start = WallClock() + std::chrono::hours(1);
    Pokemon::ScreenLook out;
    out.log.record({}, start);
    if (std::exchange(m_dialog_rose, false)){
        out.log.record({ScreenEvent::DIALOG_BOX}, start + Milliseconds(33));
    }
    out.window = {.start = start, .end = start + BETWEEN_STEPS_LOOK};
    return out;
}

StepInterruptActions StoryMapsSim::sim_interrupt_actions(){
    const auto cleared = [this](std::string_view what){
        return [this, what](const Pokemon::InterruptVerdict&) -> std::optional<StoryFailure>{
            m_dialog_notes.push_back(std::format("{} cleared before the next step.", what));
            m_dialog_up = false;
            return std::nullopt;
        };
    };
    return StepInterruptActions{
        .wild = cleared("a wild battle"),
        .trainer = cleared("a trainer's battle"),
        .dialog = cleared("a dialog"),
        .warp = cleared("a warp"),
    };
}

FrlgStepAction StoryMapsSim::action(StoryActionId id){
    switch (id){
    case StoryActionId::ROUTE:
        return [this](const StoryStep& step, FrlgRunState& state){ return walk_to(state, step); };
    case StoryActionId::LEGS:
        return [this](const StoryStep& step, FrlgRunState& state){ return legs(state, step); };
    case StoryActionId::STEP:
        return [this](const StoryStep& step, FrlgRunState& state){ return step_ahead(state, step); };
    case StoryActionId::READY:
        return [this](const StoryStep& step, FrlgRunState& state) -> std::optional<StoryFailure>{
            FrlgRunState visit = state;
            return walk_to(visit, step);
        };
    case StoryActionId::SHOP:
        return [this](const StoryStep& step, FrlgRunState&){ return shop(step); };
    case StoryActionId::PUSH_BOULDER:
        return [this](const StoryStep& step, FrlgRunState& state){ return boulder_pushed(state, step, maps(state)); };
    case StoryActionId::CUT_TREE:
        return m_stubs.stub([this](const StoryStep& step, FrlgRunState& state){ return tree_cut(state, step, maps(state)); });
    case StoryActionId::CLOSE_TRAINER_CARD:
    case StoryActionId::LEAVE_GIFT_SUMMARY:
    case StoryActionId::TRASH_CANS:
    case StoryActionId::GAME:
        return m_stubs.stub([](const StoryStep& step, FrlgRunState& state) -> std::optional<StoryFailure>{
            state.pose = paced_pose(state.pose, step);
            return std::nullopt;
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

std::optional<StoryFailure> StoryMapsSim::legs(FrlgRunState& state, const StoryStep& step) const{
    const Standing at = standing(state.pose, maps(state));
    if (!at.error.empty()){
        return frlg_failure("no_pose", at.error);
    }
    const std::optional<std::vector<RouteStep>> route = fixed_route(
        resolve_maps(maps(state), state.gates), at.position, frlg_arg<std::vector<StoryLeg>>(step.args, "legs")
    );
    if (!route || route->empty()){
        return frlg_failure("bad_legs", to_string(at.position));
    }
    state.pose = to_grid_pose(StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after});
    return std::nullopt;
}

std::optional<StoryFailure> StoryMapsSim::step_ahead(FrlgRunState& state, const StoryStep& step) const{
    const std::optional<StoryPose> at = story_pose(state.pose);
    if (!at){
        return frlg_failure("no_pose", format_grid_pose(state.pose));
    }
    const Direction direction = frlg_arg<Direction>(step.args, "direction");
    const MapData* map = map_named(at->map);
    if (step.variant != TRAINER_LINE_VARIANT || map == nullptr){
        state.pose = to_grid_pose(StoryPose{at->map, at->tile.next(direction), direction});
        return std::nullopt;
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
        return frlg_failure("no_trainer", to_string(MapPosition{at->map, onto}));
    }
    state.pose = to_grid_pose(StoryPose{at->map, onto, direction});
    return std::nullopt;
}

std::optional<StoryFailure> StoryMapsSim::shop(const StoryStep& step){
    const std::vector<Pokemon::StoryItem>& items = frlg_arg<std::vector<Pokemon::StoryItem>>(step.args, "items");
    static const std::vector<std::string> LIST{"poke_ball", "potion", "antidote", "parlyz_heal", "cancel"};
    const std::optional<std::vector<MartOrder>> orders = viridian_mart_orders(items);
    if (!orders){
        return frlg_failure("unknown_item", step.name);
    }
    int cursor = 0;
    for (size_t c = 0; c < orders->size(); c++){
        const MartOrder& order = (*orders)[c];
        cursor += order.row - cursor;
        if (cursor < 0 || cursor >= static_cast<int>(LIST.size()) || LIST[cursor] == "cancel"){
            return frlg_failure("mart_cancel", std::format("order {} ends on row {}", c + 1, cursor + 1));
        }
        if (LIST[cursor] != items[c].item){
            return frlg_failure("wrong_item", std::format("{} instead of {}", LIST[cursor], items[c].item));
        }
        m_bag[LIST[cursor]] += order.quantity;
    }
    return std::nullopt;
}

const std::vector<MapData>& StoryMapsSim::maps(const FrlgRunState& state) const{
    return m_knobs.maps != nullptr ? *m_knobs.maps : run_story_maps(state.flags);
}

std::optional<StoryFailure> StoryMapsSim::walk_to(FrlgRunState& state, const StoryStep& step) const{
    const Pokemon::GateRules rules = frlg_gate_rules(state);
    if (state.pose && state.pose->tiles.size() > 1){
        const std::optional<StoryPose> target = story_pose(frlg_arg<Pokemon::GridPose>(step.args, "target"));
        const RouteGoal goal{{target->map, target->tile}, target->facing};
        const std::optional<std::vector<RouteStep>> route = plan_story_route(maps(state), *state.pose, std::nullopt, goal, state.gates, rules);
        if (!route || route->empty()){
            return frlg_failure("no_route", format_grid_pose(state.pose) + " to " + to_string(goal.position));
        }
        state.gates = gates_after_route(state.gates, *route);
        state.pose = to_grid_pose(StoryPose{target->map, target->tile, route->back().facing_after});
        return std::nullopt;
    }
    const Standing at = standing(state.pose, maps(state));
    if (!at.error.empty()){
        return frlg_failure("no_pose", at.error);
    }
    const std::optional<StoryPose> target = story_pose(frlg_arg<Pokemon::GridPose>(step.args, "target"));
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route_gated(
        maps(state), at.position, goal, state.gates, rules, story_route_costs(), at.facing
    );
    if (!route){
        return frlg_failure("no_route", to_string(at.position) + " to " + to_string(goal.position));
    }
    const MapPosition end = follow_overworld_route(maps(state), at.position, *route, state.gates);
    if (!(end == goal.position)){
        return frlg_failure("wrong_end", to_string(end));
    }
    if (const std::optional<std::string> leg = overrun_wall_leg(*route, maps(state))){
        return frlg_failure("wall_overrun", *leg);
    }
    state.gates = gates_after_route(state.gates, *route);
    const Direction facing = route->empty() ? at.facing : route->back().facing_after;
    state.pose = to_grid_pose(StoryPose{target->map, target->tile, facing});
    return std::nullopt;
}


std::optional<std::vector<RouteStep>> walked_route(
    const StoryPose& at, const StoryStep& step,
    const std::vector<MapData>& maps, const Pokemon::GateState& gates, const Pokemon::GateRules& rules
){
    const MapPosition start{at.map, at.tile};
    if (step.action == "legs"){
        std::optional<std::vector<RouteStep>> route = fixed_route(
            resolve_maps(maps, gates), start, frlg_arg<std::vector<StoryLeg>>(step.args, "legs")
        );
        if (route && step.variant == TAPS_VARIANT){
            for (RouteStep& leg : *route){
                leg.tapped = true;
            }
        }
        return route;
    }
    const std::optional<StoryPose> target = story_pose(frlg_arg<Pokemon::GridPose>(step.args, "target"));
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    return plan_overworld_route_gated(maps, start, goal, gates, rules, story_route_costs(), at.facing);
}


namespace{

constexpr int MAX_SIM_BOXES = 5;

Pokemon::ScreenLookFunction sim_screen_look(OverworldSim& sim){
    return [&sim, from = sim.state().time](bool fresh) mutable{
        if (fresh){
            from = sim.state().time;
        }
        sim.watch({}, BETWEEN_STEPS_LOOK);
        return Pokemon::ScreenLook{
            .log = screen_event_log(sim.state(), from),
            .window = {.start = sim_wall_clock(from), .end = sim_wall_clock(sim.state().time)},
        };
    };
}

std::string battle_end_on_sim(const OverworldSim& sim){
    constexpr Milliseconds POLL(300);
    constexpr Milliseconds BAG(4000);
    constexpr Milliseconds LAST_TEXT(10000);
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

void sim_wait_out_battle_exit(OverworldSim& sim){
    wait_for_quiet_overworld(sim_screen_look(sim), MovementMechanics::AFTER_BATTLE_QUIET, BATTLE_EXIT_TIMEOUT);
}

void sim_wait_out_warp(OverworldSim& sim){
    constexpr Milliseconds POLL(100);
    constexpr Milliseconds FADE_TIMEOUT(8000);
    for (Milliseconds waited(0); sim.state().screen == SimScreen::FADE && waited < FADE_TIMEOUT; waited += POLL){
        sim.watch({}, POLL);
    }
    sim.watch({}, OverworldWalkSteps::AFTER_WARP);
}

class SimBoulderPresses : public BoulderPresses{
public:
    explicit SimBoulderPresses(OverworldSim& sim)
        : m_sim(sim)
    {}

    BoulderPressRead press(Direction direction) override{
        if (m_sim.state().facing != direction){
            sim_turn_to(m_sim, direction);
        }
        const MapPosition from = m_sim.state().position;
        const TilePos ahead = from.point.next(direction);
        const bool boulder_ahead = std::any_of(m_sim.objects().begin(), m_sim.objects().end(), [&](const MapObject& object){
            return object.name == "pushable_boulder" && object.x == ahead.x && object.y == ahead.y;
        });
        if (boulder_ahead){
            return {.scrolled = false, .boulder_moved = m_sim.use_field(SimField::PUSH_BOULDER)};
        }
        sim_step_once(m_sim, direction);
        return {.scrolled = !(m_sim.state().position == from), .boulder_moved = false};
    }

    bool clear_interrupt() override{
        const Pokemon::ScreenSettle settle = wait_for_quiet_overworld(sim_screen_look(m_sim), MovementMechanics::AFTER_BATTLE_QUIET, BATTLE_EXIT_TIMEOUT);
        return quiet_overworld(settle, MovementMechanics::AFTER_BATTLE_QUIET);
    }

private:
    OverworldSim& m_sim;
};

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
        sim_wait_out_warp(sim);
    }
    return "";
}

bool center_walkable(const StoryStep& step){
    if (!is_center_variant(step.action)){
        return false;
    }
    const std::optional<StoryPose> after = step.pose_after ? story_pose(*step.pose_after) : std::nullopt;
    const std::vector<std::pair<std::string, std::string>>& maps = overworld_map_json();
    return after && std::any_of(maps.begin(), maps.end(), [&](const auto& map){ return map.first == after->map; });
}

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


OverworldStepSim::OverworldStepSim(OverworldSim& sim, std::string row, StoryPose at)
    : m_sim(sim)
    , m_at(std::move(at))
    , m_table(make_story_action_table([this](StoryActionId id){ return action(id); }))
{
    m_stubs.begin_row(std::move(row));
}

std::optional<StoryFailure> OverworldStepSim::run(const StoryStep& step, FrlgRunState& state){
    const auto it = m_table.find(step.action);
    if (it == m_table.end()){
        return frlg_unsupported_step(step);
    }
    return it->second(step, state);
}

FrlgStepAction OverworldStepSim::action(StoryActionId id){
    FrlgStepAction played_action = step_action(id);
    return [this, act = std::move(played_action)](const StoryStep& step, FrlgRunState& state){
        clear_box_before_a(step, sim_screen_look(m_sim), [this]{
            for (int c = 0; c < MAX_SIM_BOXES && m_sim.state().screen == SimScreen::MESSAGE; c++){
                m_sim.close_message();
            }
        });
        return act(step, state);
    };
}

FrlgStepAction OverworldStepSim::step_action(StoryActionId id){
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
    return [this, body](const StoryStep& step, FrlgRunState& state) -> std::optional<StoryFailure>{
        m_flags = state.flags;
        m_beaten = beaten_trainers(state.flags);
        const std::string failure = (this->*body)(step);
        state.flags = m_flags;
        if (failure.empty()){
            return std::nullopt;
        }
        return frlg_failure("sim_failure", failure);
    };
}

std::string OverworldStepSim::walk(const StoryStep& step){
    const std::vector<MapData>& maps = run_story_maps(m_flags);
    const Pokemon::GateState& gates = m_sim.state().gates;
    const std::optional<std::vector<RouteStep>> route = walked_route(
        m_at, step, maps, gates, Pokemon::GateRules{.card_key_held = m_sim.card_key_held()}
    );
    if (route && route->empty()){
        return "";
    }
    const RoutePlanning planning{
        .maps = &maps,
        .gates = gates,
        .costs = story_route_costs(),
        .start = MapPosition{m_at.map, m_at.tile},
        .label = step.name,
        .beaten_trainers = m_beaten,
    };
    SimWalkerRules rules;
    rules.trainer_battle_end = battle_end_on_sim;
    if (move_speed(step) == MoveSpeed::BIKE && m_sim.state().on_bike){
        rules.speed = MoveSpeed::BIKE;
    }
    const std::string failure = route ? OverworldSimWalker(m_sim, rules).run(*route, planning) : "no legs.";
    m_at = route ? StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after} : m_at;
    return failure;
}

std::string OverworldStepSim::step_ahead(const StoryStep& step){
    const Direction direction = frlg_arg<Direction>(step.args, "direction");
    if (step.variant.empty() && step.pose_after){
        if (const std::optional<RouteStep> warp = warp_press(story_maps(), MapPosition{m_at.map, m_at.tile}, direction)){
            const std::string failure = step_onto_warp(*warp);
            m_at = *story_pose(*step.pose_after);
            return failure;
        }
        return step_onto(step);
    }
    if (step.variant.empty()){
        if (const std::optional<RouteStep> warp = warp_press(story_maps(), MapPosition{m_at.map, m_at.tile}, direction)){
            return step_onto_warp(*warp);
        }
    }
    const auto tap = [this, direction]{
        sim_step_once(m_sim, direction);
    };
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
    const int tiles = step.variant == TRAINER_LINE_VARIANT ? TRAINER_LINE_TILES : 1;
    if (!step_until_seen(tiles, tap, seen)){
        return "no trainer saw the player.";
    }
    if (step.variant == TRAINER_LINE_VARIANT || step.variant == TRAINER_WAIT_VARIANT){
        note_trainer_walked_up(m_sim.state().position, m_flags);
    }
    return "";
}

std::string OverworldStepSim::step_onto_warp(const RouteStep& warp){
    const int warps_before = m_sim.state().warps;
    sim_step_once(m_sim, warp.direction);
    for (int tap = 0; tap < 2 && m_sim.state().warps == warps_before; tap++){
        sim_step_once(m_sim, warp.direction);
    }
    if (m_sim.state().warps == warps_before){
        return std::format("no fade in and out after stepping onto the warp to {}.", warp.end.map);
    }
    sim_wait_out_warp(m_sim);
    return "";
}

std::string OverworldStepSim::step_onto(const StoryStep& step){
    sim_step_once(m_sim, frlg_arg<Direction>(step.args, "direction"));
    m_at = *story_pose(*step.pose_after);
    return m_sim.state().position == MapPosition{m_at.map, m_at.tile} ? "" : "expected to end on " + to_string(MapPosition{m_at.map, m_at.tile}) + ".";
}

std::string OverworldStepSim::cut(const StoryStep& step){
    const Direction direction = frlg_arg<Direction>(step.args, "direction");
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

std::string OverworldStepSim::push_boulder(const StoryStep& step){
    const Direction direction = frlg_arg<Direction>(step.args, "direction");
    const int64_t tiles = frlg_arg<int64_t>(step.args, "tiles");
    if (frlg_arg<bool>(step.args, "activate")){
        if (m_sim.state().facing != direction){
            sim_turn_to(m_sim, direction);
        }
        if (!m_sim.use_field(SimField::STRENGTH)){
            return std::format("no boulder {} of {}.", direction_name(direction), to_string(MapPosition{m_at.map, m_at.tile}));
        }
    }
    SimBoulderPresses presses(m_sim);
    const MapPosition start{m_at.map, m_at.tile};
    const BoulderPush push = push_boulder_tiles(direction, tiles, presses);
    m_at.facing = direction;
    for (int64_t c = 0; c < push.walked; c++){
        m_at.tile = m_at.tile.next(direction);
    }
    if (push.failure){
        return *push.failure;
    }
    if (!(m_sim.state().position == MapPosition{m_at.map, m_at.tile})){
        return std::format("expected to stand behind the boulder on {}.", to_string(MapPosition{m_at.map, m_at.tile}));
    }
    note_boulder_pushed(start, direction, tiles, m_flags);
    return "";
}

FrlgStepAction OverworldStepSim::hideout_or_stub(){
    FrlgStepAction stub = m_stubs.stub();
    FrlgStepAction played_act = [this](const StoryStep& step, FrlgRunState&) -> std::optional<StoryFailure>{
        const std::string failure = hideout(step);
        if (failure.empty()){
            return std::nullopt;
        }
        return frlg_failure("sim_failure", failure);
    };
    return [stub = std::move(stub), played_act = std::move(played_act)](const StoryStep& step, FrlgRunState& state){
        return is_hideout_step(step) || center_walkable(step) || field_move_simulated(step)
            ? played_act(step, state)
            : stub(step, state);
    };
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
    sim_wait_out_battle_exit(m_sim);
    const std::string out = sim_walk_room_legs(m_sim, lift_out_legs());
    return out.empty() ? ended_on(step) : out;
}

std::string OverworldStepSim::ended_on(const StoryStep& step){
    const SimState& state = m_sim.state();
    m_at = StoryPose{state.position.map, state.position.point, state.facing};
    if (!step.pose_after){
        return "";
    }
    const std::optional<StoryPose> expected = story_pose(*step.pose_after);
    if (!expected || !(MapPosition{expected->map, expected->tile} == state.position)){
        return "expected to end on " + (expected ? to_string(MapPosition{expected->map, expected->tile}) : std::string("a pose")) + ".";
    }
    return "";
}

std::string OverworldStepSim::battle(const StoryStep& step){
    if (!m_sim.close_message() || m_sim.handle_battle(SimBattleChoice::FIGHT) != SimBattleEnd::ENDED){
        return "no_battle.";
    }
    if (const std::string failure = battle_end_on_sim(m_sim); !failure.empty()){
        return failure;
    }
    sim_wait_out_battle_exit(m_sim);
    m_at = *story_pose(*step.pose_after);
    if (!(m_sim.state().position == MapPosition{m_at.map, m_at.tile})){
        return "expected " + to_string(MapPosition{m_at.map, m_at.tile}) + ".";
    }
    return "";
}


std::string row_step_on_sim(
    OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id,
    const std::set<std::string>& run_flags
){
    std::set<std::string> flags = run_flags;
    return row_step_carrying_flags(sim, at, step, id, flags);
}

std::string row_step_carrying_flags(
    OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id,
    std::set<std::string>& run_flags
){
    const std::string map_before = at.map;
    OverworldStepSim executor(sim, id, at);
    FrlgRunState state;
    state.flags = run_flags;
    const std::optional<StoryFailure> failure = executor.run(step, state);
    at = executor.at();
    run_flags = std::move(state.flags);
    trainers_back_after_map_change(map_before, at.map, run_flags);
    boulders_back_after_map_change(map_before, at.map, run_flags);
    if (!failure){
        if (std::find(step.flags_after.begin(), step.flags_after.end(), "card_key_held") != step.flags_after.end()){
            sim.give_card_key();
        }
        if (step.gates_after){
            sim.change_gates(*step.gates_after);
        }
        return "";
    }
    return id + "/" + step.name + ": " + failure->detail + " On " + to_string(sim.state().position) + ".";
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
