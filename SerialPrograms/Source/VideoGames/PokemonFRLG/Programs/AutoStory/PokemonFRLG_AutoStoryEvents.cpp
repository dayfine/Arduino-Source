/*  AutoStory Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <memory>
#include <set>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Navigation/Grid/Trace/Pokemon_RouteLog.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldGatedRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldReanchor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecoveryWalk.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStoryAbilities.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryBoulderPresses.h"
#include "PokemonFRLG_AutoStoryBoulders.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryCenterHeal.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryGrind.h"
#include "PokemonFRLG_AutoStoryHallOfFame.h"
#include "PokemonFRLG_AutoStoryHealing.h"
#include "PokemonFRLG_AutoStoryHealingSteps.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryLegs.h"
#include "PokemonFRLG_AutoStoryRepelStep.h"
#include "PokemonFRLG_AutoStoryRoutePose.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"
#include "PokemonFRLG_AutoStorySlopeRun.h"
#include "PokemonFRLG_AutoStoryStartMenu.h"
#include "PokemonFRLG_AutoStorySummary.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTeachScreens.h"
#include "PokemonFRLG_AutoStoryTrashCans.h"
#include "PokemonFRLG_AutoStoryViridianMart.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using Pokemon::StoryFailure;


namespace{

constexpr Milliseconds SCENE_TIMEOUT = 300000ms;
constexpr Milliseconds YES_NO_TIMEOUT = 60000ms;
constexpr Milliseconds BATTLE_TEXT_TIMEOUT = 90000ms;

template <typename Type>
const Type& arg(const StoryStep& step, std::string_view key){
    return frlg_arg<Type>(step.args, key);
}

const MapData* find_story_map(const std::string& name){
    for (const MapData& map : story_maps()){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

std::optional<Direction> face_a_wall(StoryEnv& env, const MapPosition& position){
    const MapData* map = find_story_map(position.map);
    if (map == nullptr){
        return std::nullopt;
    }
    const std::optional<Direction> wall = wall_beside(*map, position.point);
    if (wall){
        env.console.log(std::string("Story: facing unknown, turning to the wall ") + direction_name(*wall) + ".");
        face_wall(env.console, env.context, *wall, env.player_gender);
    }
    return wall;
}

void log_arrival(StoryEnv& env, const std::string& label, const std::string& map, const TilePos& tile){
    env.console.log(std::string(Pokemon::ROUTE_LOG_TAG) + Pokemon::route_arrived_line(label, {{map, tile}}));
}

WallCheck wall_check(const StoryEnv& env){
    return Pokemon::runs_check(env.checks, Pokemon::StoryCheckKind::LEG_END_TAP) ? WallCheck::TAP : WallCheck::SKIP;
}

OverworldInterruptSet walk_interrupts(StoryEnv& env, const StoryStep& step){
    OverworldInterruptSet out = story_interrupts(env.supplies, arg<bool>(step, "fight_wild"), env.checks, env.language);
    out.make_observer = []() -> std::unique_ptr<ScreenEventObserver>{
        return std::make_unique<FrlgScreenObserver>();
    };
    return out;
}

std::optional<StoryFailure> walk_route(StoryEnv& env, FrlgRunState& state, const StoryStep& step, const std::vector<MapData>& maps){
    if (!state.pose || state.pose->map.empty() || state.pose->tiles.empty()){
        return frlg_failure("no_pose", format_grid_pose(state.pose));
    }
    const std::optional<MapPosition> start = route_start(state.pose);
    std::optional<Direction> facing = state.pose->facing;
    if (!facing && start){
        facing = face_a_wall(env, *start);
    }
    if (!facing && start){
        return frlg_failure("unknown_facing", to_string(*start));
    }
    const Pokemon::GridPose& target_pose = arg<Pokemon::GridPose>(step, "target");
    const std::optional<StoryPose> target = story_pose(target_pose);
    if (!target){
        return frlg_failure("bad_target", format_grid_pose(target_pose));
    }
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    const std::optional<std::vector<RouteStep>> route = plan_story_route(
        maps, *state.pose, facing, goal, state.gates, frlg_gate_rules(state)
    );
    if (!route){
        return frlg_failure("no_route", format_grid_pose(state.pose) + " to " + to_string(goal.position));
    }
    if (!start){
        env.console.log("Story: " + step.name + ": on one of " + format_grid_pose(state.pose) + "; holds to walls first, to one tile.");
    }
    const OverworldInterruptSet interrupts = walk_interrupts(env, step);
    RouteProgress progress{.at = start, .gates = state.gates};
    const RoutePlanning planning{
        .maps = &maps,
        .gates = state.gates,
        .costs = story_route_costs(),
        .start = start,
        .label = step.name,
        .checkpoint = env.checkpoint,
        .wall_check = wall_check(env),
        .pause = &env.pause,
        .beaten_trainers = beaten_trainers(state.flags),
        .progress = &progress,
    };
    walk_keeping_state(state, &progress, [&]{
        walk_overworld_route_with_recovery(env.console, env.context, *route, planning, env.player_gender, facing, interrupts, move_speed(step));
    });
    state.gates = gates_after_route(state.gates, *route);
    log_arrival(env, step.name, target->map, target->tile);
    const std::optional<Direction> end_facing = route->empty() ? facing : std::optional<Direction>(route->back().facing_after);
    state.pose = to_grid_pose(StoryPose{target->map, target->tile, end_facing});
    return std::nullopt;
}

std::optional<StoryFailure> walk_legs(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    const std::optional<StoryPose> here = story_pose(state.pose);
    if (!here){
        return frlg_failure("no_pose", format_grid_pose(state.pose));
    }
    const MapPosition start{here->map, here->tile};
    std::optional<Direction> facing = here->facing;
    if (!facing){
        facing = face_a_wall(env, start);
    }
    std::optional<std::vector<RouteStep>> route = fixed_route(
        resolve_maps(story_maps(), state.gates), start, arg<std::vector<StoryLeg>>(step, "legs")
    );
    if (route && step.variant == TAPS_VARIANT){
        for (RouteStep& leg : *route){
            leg.tapped = true;
        }
    }
    if (!facing || !route || route->empty()){
        return frlg_failure("bad_legs", to_string(start));
    }
    const OverworldInterruptSet interrupts = walk_interrupts(env, step);
    const RoutePlanning planning{
        .maps = &story_maps(),
        .gates = state.gates,
        .costs = story_route_costs(),
        .start = start,
        .label = step.name,
        .checkpoint = env.checkpoint,
        .wall_check = wall_check(env),
        .pause = &env.pause,
        .beaten_trainers = beaten_trainers(state.flags),
    };
    walk_keeping_state(state, nullptr, [&]{
        walk_overworld_route_with_recovery(env.console, env.context, *route, planning, env.player_gender, facing, interrupts, move_speed(step));
    });
    log_arrival(env, step.name, route->back().end.map, route->back().end.point);
    state.pose = to_grid_pose(StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after});
    return std::nullopt;
}

std::optional<StoryFailure> take_warp_step(StoryEnv& env, const FrlgRunState& state, const MapPosition& from, const RouteStep& warp){
    env.context.wait_for_all_requests();
    const VideoSnapshot before = env.console.video().snapshot();
    const std::optional<Direction> facing = takes_mat_warp(story_maps(), from, warp.direction)
        ? warp.direction
        : (state.pose ? state.pose->facing : std::nullopt);
    step_once(env.console, env.context, warp.direction, facing, env.player_gender);
    OverworldWalkSteps::wait_out_warp(env.console, env.context, warp, before);
    env.console.log(std::format("Story: the step took the warp to {}.", warp.end.map));
    return std::nullopt;
}

std::optional<StoryFailure> take_step(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    const Direction direction = arg<Direction>(step, "direction");
    const std::optional<Direction> current_facing = state.pose ? state.pose->facing : std::nullopt;
    int tiles_stepped = 1;
    if (step.variant == TRAINER_LINE_VARIANT){
        const std::optional<int> stepped = step_into_line(env.console, env.context, direction, current_facing, env.player_gender, TRAINER_LINE_TILES);
        if (!stepped){
            return frlg_failure("no_trainer", std::format("{} tiles {}", TRAINER_LINE_TILES, direction_name(direction)));
        }
        env.console.log(std::format("Story: {}: a trainer saw the player after {} tile(s).", step.name, *stepped));
        tiles_stepped = *stepped;
    }else if (step.variant == TRAINER_WAIT_VARIANT){
        if (!step_into_line(env.console, env.context, direction, current_facing, env.player_gender, 1, TRAINER_WAIT_WATCH)){
            return frlg_failure("no_trainer", std::format("{} s on the tile {}", TRAINER_WAIT_WATCH.count() / 1000, direction_name(direction)));
        }
    }else if (const std::optional<StoryPose> from = story_pose(state.pose)){
        const MapPosition at{from->map, from->tile};
        if (const std::optional<RouteStep> warp = warp_press(story_maps(), at, direction)){
            return take_warp_step(env, state, at, *warp);
        }
        step_once(env.console, env.context, direction, current_facing, env.player_gender);
    }else{
        step_once(env.console, env.context, direction, current_facing, env.player_gender);
    }
    const std::optional<StoryPose> from = story_pose(state.pose);
    if (!from){
        return std::nullopt;
    }
    TilePos onto = from->tile;
    for (int c = 0; c < tiles_stepped; c++){
        onto = onto.next(direction);
    }
    state.pose = to_grid_pose(StoryPose{from->map, onto, direction});
    return std::nullopt;
}

std::optional<StoryFailure> battle(StoryEnv& env, const StoryStep& step){
    env.supplies.trainer = arg<std::string>(step, "trainer");
    env.supplies.risk = default_battle_risk(env.supplies.trainer);
    env.supplies.foe_out.clear();
    if (advance_text(env.console, env.context, BATTLE_TEXT_TIMEOUT, true) != TextEnd::BATTLE){
        return frlg_failure("no_battle");
    }
    env.supplies.foes = arg<std::vector<std::string>>(step, "foes");
    env.supplies.battle_items = arg<std::vector<Pokemon::StoryItem>>(step, "use_items");
    env.supplies.policy = battle_policy_of(step.args);
    if (!env.supplies.foes.empty()){
        std::string party;
        for (const std::string& foe : env.supplies.foes){
            party += (party.empty() ? "" : ", ") + foe;
        }
        env.console.log(std::format(
            "Story: {} is {} ({}), risk {}; attacks are chosen against the foe out.",
            step.name, env.supplies.trainer, party, battle_risk_name(env.supplies.risk)
        ));
    }
    const bool champion_win_scene = league_fight_name(env.supplies.trainer) == "champion";
    const BattleKind kind = arg<bool>(step, "run") ? BattleKind::WILD_RUN : BattleKind::TRAINER;
    const BattleEnd end = fight_battle(env.console, env.context, env.supplies, kind, env.checks, env.language);
    end_trainer_battle(env.supplies);
    if (end != BattleEnd::WON && (end != BattleEnd::LOST || !arg<bool>(step, "may_lose"))){
        return frlg_failure(end == BattleEnd::LOST ? "blackout" : end == BattleEnd::ROARED ? "league_roar_reset" : "league_early_reset");
    }
    if (end == BattleEnd::WON && champion_win_scene){
        //  Winning the Champion's battle runs straight into Oak's entrance and
        //  the Hall of Fame with no overworld START menu in between; hand off
        //  immediately to the hall_of_fame step instead of mashing past it.
        return std::nullopt;
    }
    mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
    wait_out_battle_exit(env.console, env.context);
    if (end != BattleEnd::LOST){
        recover_after_battle(env.console, env.context, env.supplies, env.checks, env.language);
    }else{
        healed_by_nurse(env.supplies);
    }
    return std::nullopt;
}

std::optional<StoryFailure> get_ready(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    Supplies& supplies = env.supplies;
    env.console.log(std::format(
        "Story: before the battle, HP {}, {}, {} Potions.",
        supplies.hp_red ? "red" : supplies.hurt ? "yellow" : "green", status_name(supplies.status), supplies.potions
    ));
    recover_after_battle(env.console, env.context, supplies, env.checks, env.language);
    if (!nurse_before_battle(supplies)){
        return std::nullopt;
    }
    if (std::optional<StoryFailure> failed = walk_route(env, state, step, run_story_maps(state.flags))){
        return failed;
    }
    heal_in_center(env);
    return std::nullopt;
}

bool talk_opens_message(ConsoleHandle& console, ProControllerContext& context){
    WhiteDialogWatcher opened(COLOR_RED);
    const int shown = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_press_button(context, BUTTON_A, 160ms, 2000ms);
        },
        {opened}
    );
    return shown >= 0;
}

constexpr int WANDERER_TALK_TRIES = 45;
bool talk_until_message(ConsoleHandle& console, ProControllerContext& context, const StoryStep& step){
    const int tries = step.variant == TALK_WANDERER_VARIANT ? WANDERER_TALK_TRIES : 1;
    for (int c = 0; c < tries; c++){
        if (talk_opens_message(console, context)){
            return true;
        }
    }
    return false;
}

bool walked(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    return !walk_route(env, state, step, run_story_maps(state.flags));
}

bool grind_heal_trip(
    StoryEnv& env, FrlgRunState& state, const StoryStep& step,
    const std::optional<Pokemon::GridPose>& patch, const GrindSpot& spot
){
    std::optional<StoryPose> here = story_pose(patch);
    const Pokemon::GridPose* nurse = game_arg<Pokemon::GridPose>(step.args, "nurse");
    const std::optional<StoryPose> back = story_pose(patch);
    if (!here || !back || nurse == nullptr){
        return false;
    }
    if (spot.displaced && spot.facing){
        here->tile = here->tile.next(*spot.facing);
    }
    if (spot.facing){
        here->facing = spot.facing;
    }
    state.pose = to_grid_pose(*here);
    StoryStep walk;
    walk.action = "route";
    walk.name = step.name + "_to_nurse";
    walk.args = {{"target", *nurse}, {"fight_wild", false}};
    if (!walked(env, state, walk)){
        return false;
    }
    heal_in_center(env);
    walk.name = step.name + "_back_to_patch";
    walk.args["target"] = Pokemon::GridPose{back->map, {back->tile}, back->facing};
    return walked(env, state, walk);
}

void pace_trainer_battle(
    StoryEnv& env, FrlgRunState& state, const StoryStep& step, const std::optional<Pokemon::GridPose>& patch
){
    clear_overworld_message(env.console, env.context, env.supplies, env.checks, env.language);
    state.pose = paced_pose(patch, step);
    StoryStep walk;
    walk.action = "route";
    walk.name = step.name + "_back_after_trainer";
    walk.args["target"] = patch ? *patch : Pokemon::GridPose{};
    walk.args["fight_wild"] = false;
    if (!walked(env, state, walk)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Pacing: no way back onto the patch after a trainer's battle.",
            env.console
        );
    }
}

std::optional<std::optional<StoryFailure>> run_abilities(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    const std::optional<Pokemon::GridPose> patch = state.pose;
    ConsoleMartActions mart(env.console, env.context, env.language, env.supplies);
    const MartSell mart_sell = [&](const MartLine& sale){
        return sell_on_console(env.console, env.context, env.language, sale);
    };
    ConsoleTeachScreens teach_screens(env.console, env.context, env.language);
    ScreenTeachActions teach(env.console.logger(), teach_screens);
    ConsoleGrindActions grind(
        env.console, env.context, env.language, env.player_gender, env.supplies, env.checks,
        [&](const GrindSpot& spot){
            return grind_heal_trip(env, state, step, patch, spot);
        },
        [&]{ pace_trainer_battle(env, state, step, patch); }
    );
    ConsoleChoiceActions choice(env.console, env.context);
    StoryAbilities abilities{
        console_member_reader(env.console, env.context, env.language, StartMenu::FULL),
        mart, mart_sell, teach, grind, choice,
        console_species_finder(env.console, env.context, env.language, env.party),
        &env.party, &env.supplies.lead,
    };
    if (const std::optional<GrindPlan> plan = step.action == GRIND_CHECK_ACTION ? grind_plan_of(step.args) : std::nullopt){
        if (grind_reached_already(env.console.logger(), grind, abilities.members, *plan)){
            state.flags.insert(step.name);
        }
        state.changed_nothing = true;
        return std::optional<StoryFailure>(std::nullopt);
    }
    StoryStep covered = step;
    if (covered.action == MART_ACTION && covered.args.contains("fill_to") && !env.supplies.counts_read){
        if (const std::vector<BagRow> rows = read_items_pocket(env.console, env.context, env.language); !rows.empty()){
            set_from_bag(env.supplies, rows);
        }
    }
    if (fill_mart_to(env.console.logger(), env.supplies, covered)){
        state.changed_nothing = true;
        return std::optional<StoryFailure>(std::nullopt);
    }
    return run_ability_game_step(env.console.logger(), &env.console, covered, abilities, state.changed_nothing);
}

std::optional<StoryFailure> cut_tree(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    const std::optional<StoryPose> here = story_pose(state.pose);
    if (!here){
        return frlg_failure("no_pose", format_grid_pose(state.pose));
    }
    const Direction direction = arg<Direction>(step, "direction");
    if (here->facing != direction){
        face_wall(env.console, env.context, direction, env.player_gender);
    }
    pbf_press_button(env.context, BUTTON_A, 160ms, 800ms);
    answer_yes_no(env.console, env.context, true, BUTTON_B, YES_NO_TIMEOUT);
    mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
    step_once(env.console, env.context, direction, direction, env.player_gender);
    return tree_cut(state, step, story_maps());
}

std::optional<StoryFailure> push_boulder(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    const std::optional<StoryPose> here = story_pose(state.pose);
    if (!here){
        return frlg_failure("no_pose", format_grid_pose(state.pose));
    }
    const Direction direction = arg<Direction>(step, "direction");
    const int64_t tiles = arg<int64_t>(step, "tiles");
    if (here->facing != direction){
        face_wall(env.console, env.context, direction, env.player_gender);
    }
    if (arg<bool>(step, "activate")){
        if (const std::optional<std::string> failure = use_strength(env.console, env.context, MapPosition{here->map, here->tile}, direction)){
            return frlg_failure("boulder_push", *failure);
        }
    }
    ConsoleBoulderPresses presses(env.console, env.context, env.player_gender, env.supplies, env.language);
    return boulder_push_outcome(step, story_maps(), push_boulder_tiles(direction, tiles, presses), state);
}

class ConsoleTrashCans{
public:
    ConsoleTrashCans(StoryEnv& env, FrlgRunState& state, const StoryStep& step)
        : m_env(env), m_state(state), m_step(step)
    {}

    CanPages check(int can){
        const TrashCanStand stand = trash_can_stand(can);
        const std::optional<StoryPose> here = story_pose(m_state.pose);
        const Pokemon::GridPose target{m_state.pose ? m_state.pose->map : std::string(), {stand.tile}, stand.facing};
        if (!here || !(here->tile == stand.tile) || here->facing != stand.facing){
            StoryStep walk = m_step;
            walk.action = "route";
            walk.variant.clear();
            walk.name = m_step.name + "_can" + std::to_string(can);
            walk.args = {{"target", target}, {"fight_wild", false}};
            static const std::vector<MapData> MAPS = trash_can_maps(story_maps());
            if (walk_route(m_env, m_state, walk, MAPS)){
                return CanPages::FAILED;
            }
        }
        const std::optional<size_t> pages = talk_and_count_pages(m_env.console, m_env.context);
        if (!pages){
            return CanPages::FAILED;
        }
        m_env.console.log(std::format("Story: {}: can {}, {} page(s).", m_step.name, can, *pages));
        return *pages == 1 ? CanPages::ONE : CanPages::TWO;
    }

private:
    StoryEnv& m_env;
    FrlgRunState& m_state;
    const StoryStep& m_step;
};

std::optional<StoryFailure> open_gym_door(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    if (!story_pose(state.pose)){
        return frlg_failure("no_pose", format_grid_pose(state.pose));
    }
    ConsoleTrashCans cans(env, state, step);
    const TrashCanResult result = run_trash_cans(env.console, [&](int can){ return cans.check(can); });
    if (result.end != TrashCanEnd::OPENED){
        return frlg_failure(std::format("trash_cans_{}", trash_can_end_name(result.end)), std::format("{} checks", result.checks));
    }
    return std::nullopt;
}

std::optional<StoryFailure> nugget_farm(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    const int64_t step_count = arg<int64_t>(step, "count");
    const int64_t count = StoryExecutor::effective_nugget_farm_count(env.nugget_farm_count, step_count);
    if (count <= 0){
        return std::nullopt;
    }
    const Pokemon::GridPose& nurse_input = arg<Pokemon::GridPose>(step, "nurse");
    const std::optional<StoryPose> nurse_pose = story_pose(nurse_input);
    if (!nurse_pose){
        return frlg_failure("bad_nurse", format_grid_pose(nurse_input));
    }
    const Pokemon::GridPose approach{"Route24", {TilePos{11, 15}}, Direction::NORTH};
    for (int64_t i = 0; i < count; i++){
        StoryStep walk;
        walk.action = "route";
        walk.name = step.name + "_walk";
        walk.args = {{"target", approach}, {"fight_wild", false}};
        if (std::optional<StoryFailure> failed = walk_route(env, state, walk, run_story_maps(state.flags))){
            return failed;
        }
        step_once(env.console, env.context, Direction::NORTH, Direction::NORTH, env.player_gender);
        if (advance_text(env.console, env.context, BATTLE_TEXT_TIMEOUT, true) != TextEnd::BATTLE){
            return frlg_failure("no_battle");
        }
        const BattleEnd end = fight_battle(env.console, env.context, env.supplies, BattleKind::TRAINER, env.checks, env.language);
        end_trainer_battle(env.supplies);
        if (end != BattleEnd::LOST){
            return frlg_failure("nugget_farm_not_lost");
        }
        mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
        wait_out_battle_exit(env.console, env.context);
        healed_by_nurse(env.supplies);
        state.pose = to_grid_pose(*nurse_pose);
    }
    state.pose = to_grid_pose(*nurse_pose);
    return std::nullopt;
}

std::optional<StoryFailure> other_game_step(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    if (step.action == NUGGET_FARM_ACTION){
        return nugget_farm(env, state, step);
    }
    const std::optional<Pokemon::GridPose> patch = state.pose;
    ConsoleCaptureActions capture(env.console, env.context, env.language, env.player_gender, [&]{ pace_trainer_battle(env, state, step, patch); });
    ConsolePcStorageActions pc(env.console, env.context, env.language);
    std::optional<std::optional<StoryFailure>> done = run_field_game_step(
        env.console, env.context, env.language, env.player_gender, step, env.party
    );
    if (done && !*done){
        if (const std::optional<std::string> repel = repel_put_on_by(step)){
            env.supplies.repel = *repel;
            env.supplies.repel_menu = step_start_menu(step.args);
        }
    }
    if (!done){
        done = run_storage_game_step(env.supplies.protected_slots, env.console.logger(), &env.console, step, capture, pc);
        if (done){
            env.party.invalidate();
        }
    }
    if (!done){
        done = run_healing_step(env, state, step);
    }
    if (!done){
        done = run_abilities(env, state, step);
    }
    if (!done){
        SlopeEnv slope{env.console, env.context, env.supplies, env.checks, env.language, env.counted_items};
        done = run_slope_step(slope, step);
    }
    if (done){
        if (!*done){
            state.pose = paced_pose(state.pose, step);
        }
        return *done;
    }
    return frlg_unsupported_step(step);
}

using ConsoleAction = std::optional<StoryFailure> (*)(StoryEnv& env, FrlgRunState& state, const StoryStep& step);

ConsoleAction game_console_action(StoryActionId id){
    switch (id){
    case StoryActionId::CLOSE_TRAINER_CARD:
        return [](StoryEnv& env, FrlgRunState&, const StoryStep&) -> std::optional<StoryFailure>{
            close_trainer_card(env.console, env.context);
            return std::nullopt;
        };
    case StoryActionId::LEAVE_GIFT_SUMMARY:
        return [](StoryEnv& env, FrlgRunState&, const StoryStep&) -> std::optional<StoryFailure>{
            leave_gift_summary(env.console, env.context);
            return std::nullopt;
        };
    case StoryActionId::CUT_TREE:
        return cut_tree;
    case StoryActionId::PUSH_BOULDER:
        return push_boulder;
    case StoryActionId::TRASH_CANS:
        return open_gym_door;
    case StoryActionId::GAME:
        return other_game_step;
    default:
        return nullptr;
    }
}

ConsoleAction console_action_of(StoryActionId id){
    if (const ConsoleAction game = game_console_action(id)){
        return game;
    }
    switch (id){
    case StoryActionId::NEW_GAME:
        return [](StoryEnv& env, FrlgRunState&, const StoryStep&) -> std::optional<StoryFailure>{
            play_new_game_intro(env.console, env.context, env.player_name, env.player_gender);
            return std::nullopt;
        };
    case StoryActionId::ROUTE:
        return [](StoryEnv& env, FrlgRunState& state, const StoryStep& step){
            return walk_route(env, state, step, run_story_maps(state.flags));
        };
    case StoryActionId::LEGS:
        return walk_legs;
    case StoryActionId::STEP:
        return [](StoryEnv& env, FrlgRunState& state, const StoryStep& step){
            std::optional<StoryFailure> failure = take_step(env, state, step);
            note_line_trainer(env.console.logger(), step, failure, state);
            return failure;
        };
    case StoryActionId::TALK:
        return [](StoryEnv& env, FrlgRunState&, const StoryStep& step) -> std::optional<StoryFailure>{
            if (!talk_until_message(env.console, env.context, step)){
                return frlg_failure("no_message", "talk: A opened no message; nothing to talk to or pick up here.");
            }
            return std::nullopt;
        };
    case StoryActionId::ANSWER:
        return [](StoryEnv& env, FrlgRunState&, const StoryStep& step) -> std::optional<StoryFailure>{
            answer_yes_no(env.console, env.context, arg<bool>(step, "yes"), BUTTON_B, YES_NO_TIMEOUT);
            return std::nullopt;
        };
    case StoryActionId::MASH:
        return [](StoryEnv& env, FrlgRunState&, const StoryStep&) -> std::optional<StoryFailure>{
            mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
            return std::nullopt;
        };
    case StoryActionId::BATTLE:
        return [](StoryEnv& env, FrlgRunState&, const StoryStep& step){ return battle(env, step); };
    case StoryActionId::HEAL:
        return [](StoryEnv& env, FrlgRunState&, const StoryStep&) -> std::optional<StoryFailure>{
            heal_in_center(env);
            return std::nullopt;
        };
    case StoryActionId::SHOP:
        return [](StoryEnv& env, FrlgRunState&, const StoryStep& step){ return viridian_shop(env, step); };
    case StoryActionId::READY:
        return get_ready;
    default:
        break;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No console action for {}", story_action_name(id)));
}

} // namespace


const RouteCosts& story_route_costs(){
    static const RouteCosts COSTS = []{
        RouteCosts costs = default_route_costs();
        costs[RouteCost::SURF] = -1;
        costs[RouteCost::LINE_TILE] = 100;
        costs[RouteCost::OPEN_CHECK] = 0;
        return costs;
    }();
    return COSTS;
}

std::string save_place_error(const std::string& start_map, const std::string& place){
    const std::optional<std::string> expected = save_card_place(start_map);
    if (!expected || place.empty() || place == *expected){
        return "";
    }
    return std::format("The save is in {}, but the checkpoint starts in {} ({}).", place, *expected, start_map);
}

std::string start_pose_mismatch(const Pokemon::GridPose& start, const StoryPose& stands){
    const std::optional<StoryPose> at = story_pose(start);
    if (at && at->map == stands.map && at->tile == stands.tile
        && (!at->facing || !stands.facing || at->facing == stands.facing)
    ){
        return "";
    }
    return "The save stands at " + format_grid_pose(to_grid_pose(stands))
        + ", but the checkpoint starts at " + format_grid_pose(start) + ".";
}

std::optional<StoryFailure> run_start_pose_check(
    const std::optional<Pokemon::GridPose>& start, const SavePoseReader& read, const StoryLog& log
){
    if (!start){
        return std::nullopt;
    }
    const StartReader reader = [start = *start, read]() -> StartRead{
        const std::optional<StoryPose> stands = read();
        if (!stands){
            return {};
        }
        const std::string mismatch = start_pose_mismatch(start, *stands);
        return mismatch.empty() ? StartRead{StartFound::AT_START} : StartRead{StartFound::ELSEWHERE, mismatch};
    };
    return run_start_check(start, reader, log);
}

std::optional<Pokemon::GridPose> paced_pose(const std::optional<Pokemon::GridPose>& patch, const StoryStep& step){
    std::optional<Direction> pace;
    if (const std::optional<GrindPlan> grind = step.action == GRIND_ACTION ? grind_plan_of(step.args) : std::nullopt){
        pace = grind->pace;
    }else if (const std::optional<CapturePlan> capture = step.action == CAPTURE_ACTION ? capture_plan_of(step.args) : std::nullopt){
        pace = capture->pace;
    }
    const std::optional<StoryPose> at = story_pose(patch);
    if (!pace || !at){
        return patch;
    }
    return Pokemon::GridPose{
        .map = at->map,
        .tiles = {at->tile, at->tile.next(*pace)},
        .facing = std::nullopt,
    };
}

std::optional<std::vector<RouteStep>> plan_story_route(
    const std::vector<MapData>& maps, const Pokemon::GridPose& pose,
    std::optional<Direction> facing, const RouteGoal& goal,
    const Pokemon::GateState& gates, const Pokemon::GateRules& rules
){
    std::vector<MapPosition> tiles;
    for (const TilePos& tile : pose.tiles){
        tiles.push_back(MapPosition{pose.map, tile});
    }
    const std::optional<std::vector<RouteStep>> holds = holds_to_one_tile(maps, tiles);
    if (!holds){
        return std::nullopt;
    }
    const MapPosition from = holds->empty() ? tiles[0] : holds->back().end;
    if (!holds->empty()){
        facing = holds->back().facing_after;
    }
    std::optional<std::vector<RouteStep>> route = plan_overworld_route_gated(maps, from, goal, gates, rules, story_route_costs(), facing);
    if (route){
        route->insert(route->begin(), holds->begin(), holds->end());
    }
    return route;
}

std::optional<Direction> wall_beside(const MapData& map, const TilePos& tile){
    for (Direction direction : {Direction::WEST, Direction::EAST, Direction::NORTH, Direction::SOUTH}){
        const TilePos next = tile.next(direction);
        if (in_bounds(map, next) && tile_at(map, next) == TileKind::WALL && object_at(map, next) == nullptr){
            return direction;
        }
    }
    return std::nullopt;
}

std::optional<Pokemon::GridPose> checked_start_pose(const StoryCheckpoint& data){
    return data.live_start ? std::nullopt : data.start;
}

std::optional<Pokemon::GridPose> checkpoint_end_pose(const StoryCheckpoint& checkpoint){
    if (checkpoint.postconditions.position){
        return checkpoint.postconditions.position;
    }
    for (auto it = checkpoint.steps.rbegin(); it != checkpoint.steps.rend(); ++it){
        if (it->pose_after){
            return it->pose_after;
        }
        if (const Pokemon::GridPose* target = game_arg<Pokemon::GridPose>(it->args, "target")){
            return *target;
        }
        if (it->action == "legs"){
            return std::nullopt;
        }
    }
    return std::nullopt;
}


StoryExecutor::StoryExecutor(StoryEnv& env)
    : m_env(env)
    , m_guard(env.console.logger())
    , m_table(make_story_action_table([this](StoryActionId id){ return console_action(id); }))
    , m_look(env.console, env.context)
    , m_interrupts(step_interrupts(
        [this](bool fresh){ return m_look.look(fresh); },
        console_interrupt_actions(env.console, env.context, [&env]{
            return story_interrupts(env.supplies, false, env.checks, env.language);
        })
    ))
{}

bool StoryExecutor::master_ball_skipped(bool take_master_ball, Pokemon::StoryCheckLevel checks){
    return !take_master_ball || checks == Pokemon::StoryCheckLevel::SPEEDRUN;
}

bool StoryExecutor::nugget_farm_skipped(uint16_t nugget_farm_count){
    return nugget_farm_count == 0;
}

int64_t StoryExecutor::effective_nugget_farm_count(uint16_t nugget_farm_count, int64_t step_count){
    if (nugget_farm_count == 0){
        return 0;
    }
    (void)step_count;
    return static_cast<int64_t>(nugget_farm_count);
}

bool StoryExecutor::holds(std::string_view condition, const FrlgRunState& state) const{
    if (condition == "master_ball_skipped"){
        return master_ball_skipped(m_env.take_master_ball, m_env.checks);
    }
    if (condition == "nugget_farm_skipped"){
        return nugget_farm_skipped(m_env.nugget_farm_count);
    }
    return state.flags.contains(std::string(condition));
}

FrlgStepAction StoryExecutor::console_action(StoryActionId id){
    if (id == StoryActionId::OPTIONS){
        return [this](const StoryStep&, FrlgRunState&) -> std::optional<StoryFailure>{
            set_options(m_env.console, m_env.context, m_menu, m_env.language);
            return std::nullopt;
        };
    }
    const ConsoleAction action = console_action_of(id);
    return clearing_box_first(
        [this, action](const StoryStep& step, FrlgRunState& state){ return action(m_env, state, step); },
        m_interrupts, m_env.console.logger()
    );
}

std::optional<StoryFailure> StoryExecutor::run_step(
    std::string_view checkpoint_id, const StoryStep& step, FrlgRunState& state
){
    state.changed_nothing = false;
    if (m_pause != nullptr){
        m_pause->before_step(std::format("{}/{}", checkpoint_id, step.name));
    }
    if (!step.skip_if.empty() && holds(step.skip_if, state)){
        return std::nullopt;
    }
    const std::string prep_key = prepared_step_key(checkpoint_id, step.name);
    if (step.once_per_save && m_env.prepared.contains(prep_key)){
        m_env.console.log(std::format("Story: {} already done on this save; skipping.", prep_key));
        return std::nullopt;
    }
    if (step.check && !Pokemon::runs_check(m_env.checks, *step.check)){
        m_guard.on_check_skipped(step, Pokemon::skipped_check_line(step.name, m_env.checks));
        return std::nullopt;
    }

    const Pokemon::InterruptCheck interrupt = Pokemon::clear_story_interrupt(m_interrupts, step.name, step.action);
    m_guard.on_screen(step, interrupt.screen);
    if (interrupt.verdict.kind != Pokemon::InterruptKind::NONE){
        m_guard.on_interrupt(step, interrupt);
    }
    if (interrupt.pose_lost){
        state.pose = std::nullopt;
    }
    if (interrupt.failure){
        return interrupt.failure;
    }

    const auto it = m_table.find(step.action);
    if (it == m_table.end()){
        return frlg_unsupported_step(step);
    }

    std::optional<StoryFailure> failure = trainers_back_on_warp(
        [&]{
            return regrow_on_warp(
                story_maps(),
                [&]() -> std::optional<StoryFailure>{
                    try{
                        return it->second(step, state);
                    }catch (OperationFailedException& e){
                        m_guard.on_failure(step, e);
                        return frlg_failure("operation_failed", e.message());
                    }
                },
                state
            );
        },
        state
    );
    m_env.supplies.counts_read = m_env.supplies.counts_read
        && items_pocket_change(step.action).value_or(ItemsPocketChange::STALE) != ItemsPocketChange::STALE;
    if (failure){
        return failure;
    }
    if (step.pose_after){
        state.pose = step.pose_after;
    }
    if (step.gates_after){
        Pokemon::apply_gate_change(state.gates, *step.gates_after);
    }
    for (const std::string& flag : step.flags_after){
        state.flags.insert(flag);
    }
    if (step.once_per_save && state.changed_nothing){
        m_env.prepared.insert(prep_key);
    }
    return std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
