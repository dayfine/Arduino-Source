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
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryRowSim.h"

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
        .act = [this, then = std::move(then)](RunState& state, const StoryStep& step){
            record(step);
            return then ? then(state, step) : Pokemon::outcome_ok();
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

const MapData* map_named(const std::string& name){
    for (const MapData& map : story_maps()){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

//  Whether the player, at the end of a WALK `step`, stops there holding on:
//  the map's edge, a wall, or water to Surf onto, but no warp of any kind,
//  even one to a map that isn't loaded.
bool stops_past_end(const RouteStep& step){
    const MapData& map = *map_named(step.end.map);
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
std::optional<std::string> overrun_wall_leg(const std::vector<RouteStep>& route){
    for (const RouteStep& step : route){
        if (step.to_wall && !stops_past_end(step)){
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
Standing standing(const Pokemon::Pose& pose){
    const std::optional<StoryPose> at = story_pose(pose);
    if (!at){
        return {{}, Direction::NORTH, "no one-tile pose: " + Pokemon::to_string(pose)};
    }
    std::optional<Direction> facing = at->facing;
    if (!facing){
        const MapData* map = map_named(at->map);
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

Outcome StoryMapsSim::run(RunState& state, const StoryStep& step){
    if (m_knobs.dialog_before && m_knobs.dialog_before(m_row, step.name)){
        m_dialog_up = m_dialog_rose = true;
        m_dialog_notes.push_back(m_row + "/" + step.name + ": a dialog pops up before it.");
    }
    return Pokemon::TableExecutor::run(state, step);
}

Outcome StoryMapsSim::run_action(RunState& state, const StoryStep& step){
    if (m_dialog_up){
        if (!presses_through_text(step.kind)){
            return Pokemon::outcome_failed("dialog", step.name + ": a dialog that popped up before it is still up");
        }
        m_dialog_up = false;
    }
    return Pokemon::TableExecutor::run_action(state, step);
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
    const auto cleared = [this](const char* what){
        return [this, what](RunState&, const Pokemon::InterruptVerdict&){
            m_dialog_notes.push_back(std::string(what) + " cleared before the next step.");
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
        return played([this](RunState& state, const StoryStep& step){ return walk_to(state.pose, step); });
    case StoryActionId::LEGS:
        return played([this](RunState& state, const StoryStep& step){ return legs(state.pose, step); });
    case StoryActionId::STEP:
        return played([this](RunState& state, const StoryStep& step){ return step_ahead(state.pose, step); });
    //  The nurse's way must plan too, but the run goes on from where it stands.
    case StoryActionId::READY:
        return played([this](RunState& state, const StoryStep& step){
            const Outcome to_nurse = walk_to(state.pose, step);
            return to_nurse.succeeded() ? Pokemon::outcome_ok() : to_nurse;
        });
    case StoryActionId::SHOP:
        return played([this](RunState&, const StoryStep& step){ return shop(step); });
    //  Pacing leaves the player on either of its tiles, as on the console.
    case StoryActionId::CLOSE_TRAINER_CARD:
    case StoryActionId::LEAVE_GIFT_SUMMARY:
    case StoryActionId::CUT_TREE:
    case StoryActionId::TRASH_CANS:
    case StoryActionId::GAME:
        return m_stubs.stub([](RunState& state, const StoryStep& step){
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
    const Standing at = standing(pose);
    if (!at.error.empty()){
        return Pokemon::outcome_failed("no_pose", at.error);
    }
    const std::optional<std::vector<RouteStep>> route = fixed_route(story_maps(), at.position, step.legs);
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
    const MapData* map = map_named(at->map);
    if (step.variant != TRAINER_LINE_VARIANT || map == nullptr){
        return Pokemon::outcome_ok(to_pose(StoryPose{at->map, at->tile.next(step.direction), step.direction}));
    }
    TilePos onto = at->tile;
    const std::optional<int> stepped = step_until_seen(
        TRAINER_LINE_TILES,
        [&]{ onto = onto.next(step.direction); },
        [&]{
            return std::any_of(map->objects.begin(), map->objects.end(), [&](const MapObject& object){
                return object.trainer && trainer_may_see(*map, object, onto);
            });
        }
    );
    if (!stepped){
        return Pokemon::outcome_failed("no_trainer", to_string(MapPosition{at->map, onto}));
    }
    return Pokemon::outcome_ok(to_pose(StoryPose{at->map, onto, step.direction}));
}

//  The Viridian Mart's list as the game shows it. After a purchase the
//  cursor stays on the row bought (console B, 2026-11-01), so each
//  order moves it from there, as buy_items() does since ad5d40885.
Outcome StoryMapsSim::shop(const StoryStep& step){
    static const std::vector<std::string> LIST{"poke_ball", "potion", "antidote", "parlyz_heal", "cancel"};
    const std::optional<std::vector<MartOrder>> orders = viridian_mart_orders(step.items);
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
        if (LIST[cursor] != step.items[c].item){
            return Pokemon::outcome_failed("wrong_item", std::format("{} instead of {}", LIST[cursor], step.items[c].item));
        }
        m_bag[LIST[cursor]] += order.quantity;
    }
    return Pokemon::outcome_ok();
}

Outcome StoryMapsSim::walk_to(const Pokemon::Pose& pose, const StoryStep& step) const{
    if (Pokemon::tiles_of(pose).size() > 1){
        //  From the tiles a pacing step leaves: the route must plan, and
        //  test_routes_after_pacing() walks it on the sim from each.
        const std::optional<StoryPose> target = story_pose(*step.target);
        const RouteGoal goal{{target->map, target->tile}, target->facing};
        const std::optional<std::vector<RouteStep>> route = plan_story_route(story_maps(), pose, std::nullopt, goal);
        if (!route || route->empty()){
            return Pokemon::outcome_failed("no_route", Pokemon::to_string(pose) + " to " + to_string(goal.position));
        }
        return Pokemon::outcome_ok(to_pose(StoryPose{target->map, target->tile, route->back().facing_after}));
    }
    const Standing at = standing(pose);
    if (!at.error.empty()){
        return Pokemon::outcome_failed("no_pose", at.error);
    }
    const std::optional<StoryPose> target = story_pose(*step.target);
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        story_maps(), at.position, goal, story_route_costs(), at.facing
    );
    if (!route){
        return Pokemon::outcome_failed("no_route", to_string(at.position) + " to " + to_string(goal.position));
    }
    const MapPosition end = follow_overworld_route(story_maps(), at.position, *route);
    if (!(end == goal.position)){
        return Pokemon::outcome_failed("wrong_end", to_string(end));
    }
    if (const std::optional<std::string> leg = overrun_wall_leg(*route)){
        return Pokemon::outcome_failed("wall_overrun", *leg);
    }
    const Direction facing = route->empty() ? at.facing : route->back().facing_after;
    return Pokemon::outcome_ok(to_pose(StoryPose{target->map, target->tile, facing}));
}


//  The route the program walks for `step`, a ROUTE or LEGS, from `at`:
//  planned, or the legs laid out.
std::optional<std::vector<RouteStep>> walked_route(const StoryPose& at, const StoryStep& step){
    const MapPosition start{at.map, at.tile};
    if (step.kind == StoryStepKind::LEGS){
        return fixed_route(story_maps(), start, step.legs);
    }
    const std::optional<StoryPose> target = story_pose(*step.target);
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    return plan_overworld_route(story_maps(), start, goal, story_route_costs(), at.facing);
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
    case StoryActionId::BATTLE:
        body = &OverworldStepSim::battle;
        break;
    default:
        return m_stubs.stub();
    }
    return played([this, body](RunState&, const StoryStep& step){
        const std::string failure = (this->*body)(step);
        return failure.empty() ? Pokemon::outcome_ok() : Pokemon::outcome_failed("sim_failure", failure);
    });
}

//  A ROUTE or LEGS by the sim's walker, fighting a trainer who sees the
//  player on the way.
std::string OverworldStepSim::walk(const StoryStep& step){
    const std::optional<std::vector<RouteStep>> route = walked_route(m_at, step);
    if (route && route->empty()){
        //  Already there.
        return "";
    }
    const RoutePlanning planning{.maps = &story_maps(), .costs = story_route_costs(), .start = MapPosition{m_at.map, m_at.tile}, .label = step.name};
    const std::string failure = route ? OverworldSimWalker(m_sim).run(*route, planning) : "no legs.";
    m_at = route ? StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after} : m_at;
    return failure;
}

//  A hop() or step_onto() (a plain step the table says the end of); else
//  a step one tap at a time, or for a step into a trainer's line by
//  step_until_seen(), each watched for the trainer.
std::string OverworldStepSim::step_ahead(const StoryStep& step){
    if (step.variant.empty() && step.pose_after){
        return step_onto(step);
    }
    const auto tap = [this, &step]{
        if (m_sim.state().facing != step.direction){
            m_sim.tap(step.direction);
        }
        m_sim.tap(step.direction);
    };
    const auto seen = [this]{
        return m_sim.watch({SimWatcher::MESSAGE, SimWatcher::BATTLE_SCREEN}, TRAINER_LINE_WATCH).has_value();
    };
    const int tiles = step.variant == TRAINER_LINE_VARIANT ? TRAINER_LINE_TILES : 1;
    return step_until_seen(tiles, tap, seen) ? "" : "no trainer saw the player.";
}

//  A hop() or step_onto(): one step, ending where the table says.
std::string OverworldStepSim::step_onto(const StoryStep& step){
    if (m_sim.state().facing != step.direction){
        m_sim.tap(step.direction);
    }
    m_sim.tap(step.direction);
    m_at = *story_pose(*step.pose_after);
    return m_sim.state().position == MapPosition{m_at.map, m_at.tile} ? "" : "expected to end on " + to_string(MapPosition{m_at.map, m_at.tile}) + ".";
}

//  A CUT_TREE_VARIANT step: turned to the tree, cut, and a step onto its
//  tile.
std::string OverworldStepSim::cut(const StoryStep& step){
    if (m_sim.state().facing != step.direction){
        m_sim.tap(step.direction);
    }
    if (!m_sim.cut_tree()){
        return "no tree to cut.";
    }
    m_sim.tap(step.direction);
    m_at = StoryPose{m_at.map, m_at.tile.next(step.direction), step.direction};
    if (!(m_sim.state().position == MapPosition{m_at.map, m_at.tile})){
        return "expected to step onto the tree's tile " + to_string(MapPosition{m_at.map, m_at.tile}) + ".";
    }
    return "";
}

//  A battle from the trainer's message, which must be up, ending where the
//  table says.
std::string OverworldStepSim::battle(const StoryStep& step){
    if (!m_sim.close_message() || m_sim.handle_battle(SimBattleChoice::FIGHT) != SimBattleEnd::ENDED){
        return "no_battle.";
    }
    m_at = *story_pose(*step.pose_after);
    if (!(m_sim.state().position == MapPosition{m_at.map, m_at.tile})){
        return "expected " + to_string(MapPosition{m_at.map, m_at.tile}) + ".";
    }
    return "";
}


std::string row_step_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id){
    OverworldStepSim executor(sim, id, at);
    RunState state;
    const Outcome outcome = executor.run(state, step);
    at = executor.at();
    if (outcome.succeeded()){
        return "";
    }
    return id + "/" + step.name + ": " + outcome.detail + " On " + to_string(sim.state().position) + ".";
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
