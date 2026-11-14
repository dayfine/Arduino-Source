/*  AutoStory Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <memory>
#include <set>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/Navigation/Pokemon_RouteLog.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldReanchor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecoveryWalk.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryAbilities.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryGrind.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"
#include "PokemonFRLG_AutoStorySummary.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTeachScreens.h"
#include "PokemonFRLG_AutoStoryTrashCans.h"
#include "PokemonFRLG_AutoStoryViridianMart.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using Pokemon::facing_of;
using Pokemon::Outcome;
using Pokemon::RunState;


namespace{

//  The longest scenes: the old man's tutorial (about 60 lines and his
//  catching demo), the lab's Pokedex scene (about 75 lines).
constexpr Milliseconds SCENE_TIMEOUT = 300000ms;
constexpr Milliseconds YES_NO_TIMEOUT = 60000ms;
constexpr Milliseconds BATTLE_TEXT_TIMEOUT = 90000ms;

//  The engine knows nothing of consoles: a failed console step is a failed
//  Outcome, which the segment answers by reloading the save.
template <typename Body>
Outcome guarded(Body&& body){
    try{
        return body();
    }catch (OperationFailedException& e){
        return Pokemon::outcome_failed("operation_failed", e.message());
    }
}

//  The step's arg `key`, which its action declares.
template <typename Type>
const Type& arg(const StoryStep& step, std::string_view key){
    return Pokemon::story_arg<Type>(step.args, key);
}

//  A checkpoint's start as the engine's Pose: nothing for a new game.
std::optional<Pokemon::Pose> start_pose(const StoryCheckpoint& checkpoint){
    return checkpoint.start ? std::optional<Pokemon::Pose>(Pokemon::pose_of(*checkpoint.start)) : std::nullopt;
}

const MapData* find_story_map(const std::string& name){
    for (const MapData& map : story_maps()){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

//  Turns the player to a wall next to `position`, which turns without a
//  step whichever way the player faced. Nothing if no wall is next to it.
std::optional<Direction> face_a_wall(StoryEnv& env, const MapPosition& position){
    const MapData* map = find_story_map(position.map);
    if (map == nullptr){
        return std::nullopt;
    }
    const std::optional<Direction> wall = wall_beside(*map, position.point);
    if (wall){
        env.console.log(std::string("Story: facing unknown, turning to the wall ") + direction_name(*wall) + ".");
        face_wall(env.context, *wall);
    }
    return wall;
}

//  The shared route log's arrival line (Pokemon_RouteLog.h), as BDSP's walker
//  writes it: where the route was walked to.
void log_arrival(StoryEnv& env, const std::string& label, const std::string& map, const TilePos& tile){
    env.console.log(std::string(Pokemon::ROUTE_LOG_TAG) + Pokemon::route_arrived_line(label, {{map, tile}}));
}

//  The walker's check that each leg to a wall ended at it (a diagnostic):
//  with Thorough checks only, as Fast trusts the walk.
WallCheck wall_check(const StoryEnv& env){
    return checks_screen(env.checks) ? WallCheck::TAP : WallCheck::SKIP;
}

//  Where a ROUTE from `pose` starts: its tile, or nothing for a set of
//  tiles (paced_pose()), which the route's first holds bring together.
std::optional<MapPosition> route_start(const Pokemon::Pose& pose){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(pose);
    if (tiles.size() != 1){
        return std::nullopt;
    }
    return MapPosition{pose.map, tiles[0]};
}

//  The story's interrupts for `step`'s walk, with FRLG's screen observer,
//  which places a wild battle seen only after an open leg's hold
//  (OverworldInterruptSet::make_observer).
OverworldInterruptSet walk_interrupts(StoryEnv& env, const StoryStep& step){
    OverworldInterruptSet out = story_interrupts(env.supplies, arg<bool>(step, "fight_wild"), env.checks, env.language);
    out.make_observer = []() -> std::unique_ptr<ScreenEventObserver>{
        return std::make_unique<FrlgScreenObserver>();
    };
    return out;
}

//  `maps`: where the route is planned; the story's but where a step knows
//  better (the trash cans' walks, past trainers who have walked up).
Outcome walk_route(StoryEnv& env, RunState& state, const StoryStep& step, const std::vector<MapData>& maps = story_maps()){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(state.pose);
    const bool known = state.pose.certainty == Pokemon::PoseCertainty::TILE
        || (state.pose.certainty == Pokemon::PoseCertainty::SET && tiles.size() > 1);
    if (tiles.empty() || !known){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
    }
    const std::optional<MapPosition> start = route_start(state.pose);
    std::optional<Direction> facing = facing_of(state.pose);
    if (!facing && start){
        facing = face_a_wall(env, *start);
    }
    if (!facing && start){
        return Pokemon::outcome_failed("unknown_facing", to_string(*start));
    }
    const Pokemon::Pose target_pose = Pokemon::pose_of(arg<Pokemon::GridPose>(step, "target"));
    const std::optional<StoryPose> target = story_pose(target_pose);
    if (!target){
        return Pokemon::outcome_failed("bad_target", Pokemon::to_string(target_pose));
    }
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    const std::optional<std::vector<RouteStep>> route = plan_story_route(maps, state.pose, facing, goal);
    if (!route){
        return Pokemon::outcome_failed("no_route", Pokemon::to_string(state.pose) + " to " + to_string(goal.position));
    }
    if (!start){
        env.console.log("Story: " + step.name + ": on one of " + Pokemon::to_string(state.pose) + "; holds to walls first, to one tile.");
    }
    const OverworldInterruptSet interrupts = walk_interrupts(env, step);
    //  Planning lets the walker recover a stopped open leg: it plans back to
    //  the leg's end, then walks the rest as given. From a set of tiles
    //  the start isn't known: the walker reads the facing.
    const RoutePlanning planning{.maps = &maps, .costs = story_route_costs(), .start = start, .label = step.name, .checkpoint = env.checkpoint, .wall_check = wall_check(env)};
    walk_overworld_route_with_recovery(env.console, env.context, *route, planning, facing, interrupts, move_speed(step));
    log_arrival(env, step.name, target->map, target->tile);
    const std::optional<Direction> end_facing = route->empty() ? facing : std::optional<Direction>(route->back().facing_after);
    return Pokemon::outcome_ok(to_pose(StoryPose{target->map, target->tile, end_facing}));
}

Outcome walk_legs(StoryEnv& env, RunState& state, const StoryStep& step){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(state.pose);
    if (tiles.size() != 1 || state.pose.certainty != Pokemon::PoseCertainty::TILE){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
    }
    const MapPosition start{state.pose.map, tiles[0]};
    std::optional<Direction> facing = facing_of(state.pose);
    if (!facing){
        facing = face_a_wall(env, start);
    }
    const std::optional<std::vector<RouteStep>> route = fixed_route(story_maps(), start, arg<std::vector<StoryLeg>>(step, "legs"));
    if (!facing || !route || route->empty()){
        return Pokemon::outcome_failed("bad_legs", to_string(start));
    }
    const OverworldInterruptSet interrupts = walk_interrupts(env, step);
    //  Planning lets the walker recover a stopped open leg: it plans back to
    //  the leg's end, then walks the rest as given.
    const RoutePlanning planning{.maps = &story_maps(), .costs = story_route_costs(), .start = start, .label = step.name, .checkpoint = env.checkpoint, .wall_check = wall_check(env)};
    walk_overworld_route_with_recovery(env.console, env.context, *route, planning, facing, interrupts, move_speed(step));
    log_arrival(env, step.name, route->back().end.map, route->back().end.point);
    return Pokemon::outcome_ok(to_pose(StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after}));
}

Outcome take_step(StoryEnv& env, RunState& state, const StoryStep& step){
    const Direction direction = arg<Direction>(step, "direction");
    int tiles_stepped = 1;
    if (step.action == TRAINER_LINE_VARIANT){
        const std::optional<int> stepped = step_into_line(env.console, env.context, direction, facing_of(state.pose), TRAINER_LINE_TILES);
        if (!stepped){
            return Pokemon::outcome_failed("no_trainer", std::format("{} tiles {}", TRAINER_LINE_TILES, direction_name(direction)));
        }
        env.console.log(std::format("Story: {}: a trainer saw the player after {} tile(s).", step.name, *stepped));
        tiles_stepped = *stepped;
    }else if (step.action == TRAINER_WAIT_VARIANT){
        if (!step_into_line(env.console, env.context, direction, facing_of(state.pose), 1, TRAINER_WAIT_WATCH)){
            return Pokemon::outcome_failed("no_trainer", std::format("{} s on the tile {}", TRAINER_WAIT_WATCH.count() / 1000, direction_name(direction)));
        }
    }else{
        step_once(env.context, direction, facing_of(state.pose));
    }
    const std::vector<TilePos> tiles = Pokemon::tiles_of(state.pose);
    if (tiles.size() != 1){
        return Pokemon::outcome_ok();
    }
    TilePos onto = tiles[0];
    for (int c = 0; c < tiles_stepped; c++){
        onto = onto.next(direction);
    }
    return Pokemon::outcome_ok(to_pose(StoryPose{state.pose.map, onto, direction}));
}

Outcome battle(StoryEnv& env, const StoryStep& step){
    env.supplies.trainer = arg<std::string>(step, "trainer");
    env.supplies.risk = default_battle_risk(env.supplies.trainer);
    env.supplies.foe_out.clear();
    if (advance_text(env.console, env.context, BATTLE_TEXT_TIMEOUT, true) != TextEnd::BATTLE){
        return Pokemon::outcome_failed("no_battle");
    }
    env.supplies.foes = arg<std::vector<std::string>>(step, "foes");
    if (!env.supplies.foes.empty()){
        env.console.log(std::format(
            "Story: {} is {}, risk {}; attacks are chosen against the foe out.",
            step.name, env.supplies.trainer, battle_risk_name(env.supplies.risk)
        ));
    }
    const BattleEnd end = fight_battle(env.console, env.context, env.supplies, BattleKind::TRAINER, env.checks, env.language);
    env.supplies.foes.clear();
    env.supplies.trainer.clear();
    env.supplies.foe_out.clear();
    if (end == BattleEnd::LOST && !arg<bool>(step, "may_lose")){
        return Pokemon::outcome_failed("blackout");
    }
    mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
    return Pokemon::outcome_ok();
}

//  HP and status are the battle loop's last reading. Items first (an
//  Antidote, a Parlyz Heal, a Potion); then the nurse as
//  nurse_before_battle() says.
Outcome get_ready(StoryEnv& env, RunState& state, const StoryStep& step){
    Supplies& supplies = env.supplies;
    env.console.log(std::format(
        "Story: before the battle, HP {}, {}, {} Potions.",
        supplies.hp_red ? "red" : supplies.hurt ? "yellow" : "green", status_name(supplies.status), supplies.potions
    ));
    recover_after_battle(env.console, env.context, supplies, env.checks, env.language);
    if (!nurse_before_battle(supplies)){
        return Pokemon::outcome_ok();
    }
    Outcome walked = walk_route(env, state, step);
    if (!walked.succeeded()){
        return walked;
    }
    heal_at_center(env.console, env.context);
    healed_by_nurse(supplies);
    return walked;
}

} // namespace


const RouteCosts& story_route_costs(){
    static const RouteCosts COSTS = []{
        RouteCosts costs = default_route_costs();
        costs[RouteCost::SURF] = -1;
        return costs;
    }();
    return COSTS;
}

const std::vector<MapData>& story_maps(){
    static const std::vector<MapData> maps = []{
        const std::vector<MapData> all = overworld_maps();
        std::set<std::string> flags = map_flag_set(all, "after_hall_of_fame");
        flags.merge(map_flag_set(all, "new_game"));
        return overworld_maps(flags);
    }();
    return maps;
}

MoveSpeed move_speed(const StoryStep& step){
    return step.action == RUN_VARIANT ? MoveSpeed::RUN : MoveSpeed::WALK;
}

Pokemon::Pose to_pose(const StoryPose& pose){
    return Pokemon::tile_pose(pose.map, {pose.tile}, pose.facing);
}

std::optional<StoryPose> story_pose(const Pokemon::Pose& pose){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(pose);
    if (tiles.size() != 1){
        return std::nullopt;
    }
    return StoryPose{pose.map, tiles[0], facing_of(pose)};
}

std::string save_card_place(const std::string& map){
    //  A map of the place, or a building in it ("Route4_PokemonCenter_1F"
    //  is on Route 4, "Route24" isn't on Route 2).
    static const std::pair<std::string, std::string_view> PLACES[] = {
        {"PalletTown", "pallet-town"},
        {"ViridianForest", "viridian-forest"},
        {"ViridianCity", "viridian-city"},
        {"PewterCity", "pewter-city"},
        {"Route3", "route-3"},
        {"Route4", "route-4"},
        {"CeruleanCity", "cerulean-city"},
        {"Route24", "route-24"},
        {"Route25", "route-25"},
        {"VermilionCity", "vermilion-city"},
        {"Route10", "route-10"},
    };
    for (const auto& [prefix, place] : PLACES){
        if (map == prefix || map.rfind(prefix + "_", 0) == 0){
            return std::string(place);
        }
    }
    return "";
}

std::string save_place_error(const std::string& start_map, const std::string& place){
    const std::string expected = save_card_place(start_map);
    if (expected.empty() || place.empty() || place == expected){
        return "";
    }
    return "The save is in " + place + ", but the checkpoint starts in " + expected + " (" + start_map + ").";
}

std::string start_pose_mismatch(const Pokemon::Pose& start, const StoryPose& stands){
    const std::optional<StoryPose> at = story_pose(start);
    if (at && at->map == stands.map && at->tile == stands.tile
        && (!at->facing || !stands.facing || at->facing == stands.facing)
    ){
        return "";
    }
    return "The save stands at " + Pokemon::to_string(to_pose(stands))
        + ", but the checkpoint starts at " + Pokemon::to_string(start) + ".";
}

void add_start_pose_check(
    Pokemon::Checkpoint& checkpoint, const std::optional<Pokemon::Pose>& start, SavePoseReader read, StoryLog log
){
    if (!start){
        return;
    }
    StartReader reader = [start = *start, read = std::move(read)]() -> StartRead{
        const std::optional<StoryPose> stands = read();
        if (!stands){
            return {};
        }
        const std::string mismatch = start_pose_mismatch(start, *stands);
        return mismatch.empty() ? StartRead{StartFound::AT_START} : StartRead{StartFound::ELSEWHERE, mismatch};
    };
    add_start_check(checkpoint, start, std::move(reader), std::move(log));
}

std::optional<std::vector<RouteStep>> fixed_route(
    const std::vector<MapData>& maps, const MapPosition& start, const std::vector<StoryLeg>& legs
){
    const MapData* map = nullptr;
    for (const MapData& candidate : maps){
        if (candidate.name == start.map){
            map = &candidate;
        }
    }
    if (map == nullptr || !in_bounds(*map, start.point)){
        return std::nullopt;
    }
    std::vector<RouteStep> route;
    PlayerState player{start.point, elevation_at(*map, start.point), false};
    for (const StoryLeg& leg : legs){
        RouteStep step;
        step.direction = leg.direction;
        step.tiles = leg.tiles;
        step.facing_after = leg.direction;
        step.walk_only = !map->allow_running;
        for (int c = 0; c < leg.tiles; c++){
            const StepKind kind = classify_step(*map, player, leg.direction);
            const TilePos next = player.position.next(leg.direction);
            if (kind != StepKind::WALK || !in_bounds(*map, next) || warps_on_step(*map, next)){
                return std::nullopt;
            }
            step.wild |= is_tall_grass(*map, next);
            player = after_step(*map, player, leg.direction, kind);
        }
        step.to_wall = classify_step(*map, player, leg.direction) == StepKind::BLOCKED;
        step.end = MapPosition{map->name, player.position};
        route.push_back(step);
    }
    return route;
}

Pokemon::Pose paced_pose(const Pokemon::Pose& patch, const StoryStep& step){
    std::optional<Direction> pace;
    if (const std::optional<GrindPlan> grind = step.action == GRIND_ACTION ? grind_plan_of(step.args) : std::nullopt){
        pace = grind->pace;
    }else if (const std::optional<CapturePlan> capture = step.action == CAPTURE_ACTION ? capture_plan_of(step.args) : std::nullopt){
        pace = capture->pace;
    }
    const std::vector<TilePos> tiles = Pokemon::tiles_of(patch);
    if (!pace || tiles.size() != 1){
        return patch;
    }
    return Pokemon::tile_pose(patch.map, {tiles[0], tiles[0].next(*pace)}, std::nullopt);
}

std::optional<std::vector<RouteStep>> plan_story_route(
    const std::vector<MapData>& maps, const Pokemon::Pose& pose,
    std::optional<Direction> facing, const RouteGoal& goal
){
    std::vector<MapPosition> tiles;
    for (const TilePos& tile : Pokemon::tiles_of(pose)){
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
    std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, from, goal, story_route_costs(), facing);
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

namespace{

bool walked(StoryEnv& env, RunState& state, const StoryStep& step){
    const Outcome outcome = walk_route(env, state, step);
    if (outcome.succeeded() && outcome.pose){
        state.pose = *outcome.pose;
    }
    return outcome.succeeded();
}

//  A GRIND's trip from the patch to the nurse (`step`'s nurse) and back
//  to `patch`, starting where pacing left the player (`spot`).
bool grind_heal_trip(StoryEnv& env, RunState& state, const StoryStep& step, const Pokemon::Pose& patch, const GrindSpot& spot){
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
    state.pose = to_pose(*here);
    StoryStep walk;
    walk.kind = StoryStepKind::ROUTE;
    walk.name = step.name + "_to_nurse";
    walk.args = {{"target", *nurse}, {"fight_wild", false}};
    if (!walked(env, state, walk)){
        return false;
    }
    heal_at_center(env.console, env.context);
    //  PP too: a grind's battles spend more than its attacks have.
    healed_by_nurse(env.supplies);
    walk.name = step.name + "_back_to_patch";
    walk.args["target"] = Pokemon::GridPose{back->map, {back->tile}, back->facing};
    return walked(env, state, walk);
}

//  A trainer saw the player pacing `step` on `patch` (console B,
//  2026-11-03, m63: Camper Shane on Route 24): fights them, then walks back
//  onto the patch from either of pacing's tiles (see PaceTrainerBattle).
void pace_trainer_battle(StoryEnv& env, RunState& state, const StoryStep& step, const Pokemon::Pose& patch){
    clear_overworld_message(env.console, env.context, env.supplies, env.checks, env.language);
    state.pose = paced_pose(patch, step);
    StoryStep walk;
    walk.kind = StoryStepKind::ROUTE;
    walk.name = step.name + "_back_after_trainer";
    walk.args["target"] = Pokemon::GridPose{patch.map, Pokemon::tiles_of(patch), Pokemon::facing_of(patch)};
    if (!walked(env, state, walk)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Pacing: no way back onto the patch after a trainer's battle.",
            env.console
        );
    }
}

//  A MART, TEACH, LEVEL, GRIND or fossil step on the console; nothing for
//  another action.
std::optional<Outcome> run_abilities(StoryEnv& env, RunState& state, const StoryStep& step){
    const Pokemon::Pose patch = state.pose;
    ConsoleMartActions mart(env.console, env.context, env.language, env.supplies);
    ConsoleTeachScreens teach_screens(env.console, env.context, env.language);
    ScreenTeachActions teach(env.console.logger(), teach_screens);
    ConsoleGrindActions grind(
        env.console, env.context, env.language, env.supplies, env.checks,
        [&](const GrindSpot& spot){
            return grind_heal_trip(env, state, step, patch, spot);
        },
        [&]{
            pace_trainer_battle(env, state, step, patch);
        }
    );
    ConsoleChoiceActions choice(env.console, env.context);
    StoryAbilities abilities{console_member_reader(env.console, env.context, env.language), mart, teach, grind, choice};
    if (const std::optional<GrindPlan> plan = step.action == GRIND_CHECK_ACTION ? grind_plan_of(step.args) : std::nullopt){
        if (grind_reached_already(env.console.logger(), grind, abilities.members, *plan)){
            state.flags.insert(step.name);
        }
        return Pokemon::outcome_unchanged();
    }
    return run_ability_game_step(env.console.logger(), &env.console, step, abilities);
}

//  CUT_TREE_VARIANT: the tree in the step's direction, cut, and a step
//  onto its tile.
Outcome cut_tree(StoryEnv& env, RunState& state, const StoryStep& step){
    const std::optional<StoryPose> here = story_pose(state.pose);
    if (!here){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
    }
    const Direction direction = arg<Direction>(step, "direction");
    if (here->facing != direction){
        //  The tree is in the way: a tap only turns the player to it.
        face_wall(env.context, direction);
    }
    pbf_press_button(env.context, BUTTON_A, 160ms, 800ms);
    answer_yes_no(env.console, env.context, true, BUTTON_B, YES_NO_TIMEOUT);
    mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
    step_once(env.context, direction, direction);
    return Pokemon::outcome_ok(to_pose(StoryPose{here->map, here->tile.next(direction), direction}));
}

//  Vermilion Gym's trash cans on the console: a walk to each can's stand
//  (a ROUTE planned on trash_can_maps()), A, and its pages counted.
class ConsoleTrashCans{
public:
    ConsoleTrashCans(StoryEnv& env, RunState& state, const StoryStep& step)
        : m_env(env), m_state(state), m_step(step)
    {}

    CanPages check(int can){
        const TrashCanStand stand = trash_can_stand(can);
        const std::optional<StoryPose> here = story_pose(m_state.pose);
        const Pokemon::GridPose target{m_state.pose.map, {stand.tile}, stand.facing};
        if (!here || !(here->tile == stand.tile) || here->facing != stand.facing){
            StoryStep walk = m_step;
            walk.kind = StoryStepKind::ROUTE;
            walk.name = m_step.name + "_can" + std::to_string(can);
            walk.action.clear();
            walk.args = {{"target", target}, {"fight_wild", false}};
            static const std::vector<MapData> MAPS = trash_can_maps(story_maps());
            const Outcome outcome = walk_route(m_env, m_state, walk, MAPS);
            if (!outcome.succeeded()){
                return CanPages::FAILED;
            }
            if (outcome.pose){
                m_state.pose = *outcome.pose;
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
    RunState& m_state;
    const StoryStep& m_step;
};

//  TRASH_CANS_VARIANT: the search for Vermilion Gym's switches.
Outcome open_gym_door(StoryEnv& env, RunState& state, const StoryStep& step){
    if (!story_pose(state.pose)){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
    }
    ConsoleTrashCans cans(env, state, step);
    const TrashCanResult result = run_trash_cans(env.console, [&](int can){ return cans.check(can); });
    if (result.end != TrashCanEnd::OPENED){
        return Pokemon::outcome_failed(std::format("trash_cans_{}", trash_can_end_name(result.end)), std::format("{} checks", result.checks));
    }
    return Pokemon::outcome_ok(state.pose);
}

//  The GAME steps without a row of their own: a capture or a PC deposit or
//  withdrawal, a MART, TEACH, LEVEL, GRIND or fossil CHOICE.
Outcome other_game_step(StoryEnv& env, RunState& state, const StoryStep& step){
    const Pokemon::Pose patch = state.pose;
    ConsoleCaptureActions capture(env.console, env.context, env.language, [&]{
        pace_trainer_battle(env, state, step, patch);
    });
    ConsolePcStorageActions pc(env.console, env.context);
    std::optional<Outcome> done = run_storage_game_step(env.console.logger(), &env.console, step, capture, pc);
    if (!done){
        done = run_abilities(env, state, step);
    }
    if (done && done->succeeded() && !done->pose){
        //  Pacing leaves the player on either of its tiles.
        done->pose = paced_pose(state.pose, step);
    }
    if (done){
        return *done;
    }
    return Pokemon::unsupported_step(step);
}

//  How the console does an action that needs only the env.
using ConsoleAction = Outcome (*)(StoryEnv& env, RunState& state, const StoryStep& step);

//  The console's GAME actions; nullptr for another id.
ConsoleAction game_console_action(StoryActionId id){
    switch (id){
    case StoryActionId::CLOSE_TRAINER_CARD:
        return [](StoryEnv& env, RunState&, const StoryStep&){
            close_trainer_card(env.console, env.context);
            return Pokemon::outcome_ok();
        };
    case StoryActionId::LEAVE_GIFT_SUMMARY:
        return [](StoryEnv& env, RunState&, const StoryStep&){
            leave_gift_summary(env.console, env.context);
            return Pokemon::outcome_ok();
        };
    case StoryActionId::CUT_TREE:
        return cut_tree;
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
        return [](StoryEnv& env, RunState&, const StoryStep&){
            play_new_game_intro(env.console, env.context, env.player_name);
            return Pokemon::outcome_ok();
        };
    case StoryActionId::ROUTE:
        return [](StoryEnv& env, RunState& state, const StoryStep& step){ return walk_route(env, state, step); };
    case StoryActionId::LEGS:
        return walk_legs;
    case StoryActionId::STEP:
        return take_step;
    case StoryActionId::TALK:
        return [](StoryEnv& env, RunState&, const StoryStep&){
            pbf_press_button(env.context, BUTTON_A, 160ms, 800ms);
            return Pokemon::outcome_ok();
        };
    case StoryActionId::ANSWER:
        return [](StoryEnv& env, RunState&, const StoryStep& step){
            answer_yes_no(env.console, env.context, arg<bool>(step, "yes"), BUTTON_B, YES_NO_TIMEOUT);
            return Pokemon::outcome_ok();
        };
    case StoryActionId::MASH:
        return [](StoryEnv& env, RunState&, const StoryStep&){
            mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
            return Pokemon::outcome_ok();
        };
    case StoryActionId::BATTLE:
        return [](StoryEnv& env, RunState&, const StoryStep& step){ return battle(env, step); };
    case StoryActionId::HEAL:
        return [](StoryEnv& env, RunState&, const StoryStep&){
            heal_at_center(env.console, env.context);
            healed_by_nurse(env.supplies);
            return Pokemon::outcome_ok();
        };
    case StoryActionId::SHOP:
        return [](StoryEnv& env, RunState&, const StoryStep& step){ return viridian_shop(env, step); };
    case StoryActionId::READY:
        return get_ready;
    default:
        break;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No console action for {}", story_action_name(id)));
}

} // namespace


StoryExecutor::StoryExecutor(StoryEnv& env)
    : m_env(env)
    , m_guard(env.console.logger())
    , m_table(make_story_action_table([this](StoryActionId id){ return console_action(id); }))
    , m_executor(m_table, m_guard)
    , m_look(env.console, env.context)
    , m_interrupts(step_interrupts(
        [this](bool fresh){ return m_look.look(fresh); },
        console_interrupt_actions(env.console, env.context, [&env]{
            return story_interrupts(env.supplies, false, env.checks, env.language);
        })
    ))
{
    m_executor.set_interrupts(&m_interrupts);
}

Outcome StoryExecutor::run(const StoryStep& step, RunState& state){
    return m_executor.run(step, state);
}

Pokemon::StoryAction StoryExecutor::console_action(StoryActionId id){
    if (id == StoryActionId::OPTIONS){
        return {.act = [this](const StoryStep&, RunState&){
            set_options(m_env.console, m_env.context, m_menu, m_env.language);
            return Pokemon::outcome_ok();
        }, .needs_console = true};
    }
    const ConsoleAction action = console_action_of(id);
    return {.act = [this, action](const StoryStep& step, RunState& state){ return action(m_env, state, step); }, .needs_console = true};
}


namespace{

//  `first` and `second` as one checkpoint: `first`'s id, start and
//  supplies, `second`'s title and START menu, both rows' steps.
StoryCheckpoint joined_rows(const StoryCheckpoint& first, const StoryCheckpoint& second){
    StoryCheckpoint row = second;
    row.id = first.id;
    row.start = first.start;
    row.supplies = first.supplies;
    row.live_start = first.live_start;
    row.steps = first.steps;
    row.steps.insert(row.steps.end(), second.steps.begin(), second.steps.end());
    return row;
}

} // namespace

StoryRunPlan plan_story_run(
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last, const StoryHandoffs& handoffs
){
    StoryRunPlan plan;
    //  Started at the way back in from Gift RNG with its handoff on: the run
    //  takes the handoff again, from the save the row before ends at.
    if (first > 0 && first < checkpoints.size() && checkpoints[first].live_start
        && checkpoints[first - 1].handoff == StoryHandoff::GIFT_RNG && handoffs.gift_rng){
        plan.stop = StoryHandoff::GIFT_RNG;
        return plan;
    }
    for (size_t c = first; c <= last && c < checkpoints.size(); c++){
        const StoryCheckpoint& row = checkpoints[c];
        if (row.live_start && c != first){
            continue;
        }
        if (row.handoff == StoryHandoff::SID_HELPER && handoffs.sid_helper){
            plan.rows.clear();
            plan.stop = StoryHandoff::SID_HELPER;
            return plan;
        }
        if (row.handoff == StoryHandoff::STARTER_RNG && handoffs.starter_rng){
            plan.rows.emplace_back(row);
            plan.stop = StoryHandoff::STARTER_RNG;
            return plan;
        }
        if (row.handoff == StoryHandoff::GIFT_RNG && handoffs.gift_rng){
            plan.rows.emplace_back(row);
            plan.stop = StoryHandoff::GIFT_RNG;
            return plan;
        }
        if (row.handoff == StoryHandoff::STARTER_RNG && c + 1 < checkpoints.size()){
            plan.rows.emplace_back(joined_rows(row, checkpoints[c + 1]));
            c++;
            continue;
        }
        plan.rows.emplace_back(row);
    }
    return plan;
}


std::string check_save_place(StoryEnv& env, const std::string& start_map){
    if (save_card_place(start_map).empty()){
        return "";
    }
    const std::string place = read_save_place(env.console, env.context, env.language);
    if (place.empty()){
        env.console.log("Couldn't read the save card's place; going on.", COLOR_ORANGE);
        return "";
    }
    const std::string error = save_place_error(start_map, place);
    if (error.empty()){
        env.console.log("The save is in " + place + ", where the checkpoint starts.");
    }
    return error;
}

void read_lead_moves(StoryEnv& env, StartMenu menu){
    const std::optional<SummaryRead> read = read_party_summary(env.console, env.context, menu, env.language, 0);
    if (!read || read->moves.empty()){
        env.console.log("Couldn't read the lead's moves; going on with the checkpoint's.", COLOR_ORANGE);
        return;
    }
    const PartyMember& before = env.supplies.lead;
    const std::string species = read->species.empty() ? before.species : read->species;
    const uint8_t level = read->level && *read->level <= 100 ? static_cast<uint8_t>(*read->level) : before.moves_through_level;
    //  The stats belong to the species and level read.
    const bool stats_fit = species == read->species && read->level.has_value();
    env.supplies.lead = member_from_summary(species, level, read->moves, stats_fit ? read->stats : std::nullopt);
    env.saved_supplies.lead = env.supplies.lead;
}

void read_bag_supplies(StoryEnv& env, StartMenu menu){
    if (menu != StartMenu::FULL){
        return;
    }
    const std::vector<BagRow> rows = read_items_pocket(env.console, env.context, env.language);
    if (rows.empty()){
        env.console.log("Couldn't read the Items pocket; going on with the checkpoint's items.", COLOR_ORANGE);
        return;
    }
    const std::vector<std::string> unread = set_from_loaded_bag(env.supplies, env.saved_supplies, rows);
    for (const std::string& item : unread){
        env.console.log(std::format(
            "Story: couldn't read how many {} the bag holds; counting {}.", item, item_count(env.supplies, item)
        ), COLOR_ORANGE);
    }
    env.bag_read = unread.empty();
}

namespace{

//  The console's start check for `checkpoint`; with keep_start_frame (a
//  save made by hand), the screen is kept as its frame instead.
StartRead read_console_start(StoryEnv& env, const std::string& checkpoint){
    if (env.keep_start_frame){
        env.keep_start_frame = false;
        return keep_console_start(env.console, env.context, env.start_frames, checkpoint);
    }
    return check_console_start(env.console, env.context, env.start_frames, checkpoint);
}

//  After the save at the end of `ended`: the screen, kept as the start
//  frame of the table's next checkpoint, which starts there.
void keep_next_start_frame(StoryEnv& env, const std::string& ended){
    const std::vector<StoryCheckpoint>& table = story_checkpoints();
    for (size_t c = 0; c + 1 < table.size(); c++){
        if (table[c].id == ended && table[c + 1].start){
            keep_console_frame(env.console, env.context, env.start_frames, table[c + 1].id, "saved at the end of " + ended);
            return;
        }
    }
}

} // namespace

Pokemon::Checkpoint build_checkpoint(const StoryCheckpoint& data, StoryExecutor& executor){
    executor.set_menu(data.menu);
    StoryEnv& env = executor.env();
    //  The console has no reader for the tile the save stands on: the save
    //  card's place is checked when the save is loaded (check_save_place()),
    //  and the start check compares the screen with the frame kept when the
    //  program last saved there. With none kept, it passes.
    Pokemon::Checkpoint checkpoint = Pokemon::build_story_checkpoint(data, executor, &env.prepared);
    add_start_check(
        checkpoint, start_pose(data),
        [&env, id = data.id]{ return read_console_start(env, id); },
        [&env](const std::string& line){ env.console.log(line, COLOR_ORANGE); }
    );
    env.start_map = data.start ? Pokemon::grid_pose_of(*data.start).map : "";
    env.checkpoint = data.id;
    env.start_menu = data.menu;
    const StartMenu menu = data.menu;
    checkpoint.save = [&env, menu, id = data.id](RunState&){
        return guarded([&]{
            save_game(env.console, env.context, menu);
            keep_next_start_frame(env, id);
            env.has_save = true;
            env.saved_supplies = env.supplies;
            env.prepared.clear();
            env.bag_read = false;
            return Pokemon::outcome_ok();
        });
    };
    return checkpoint;
}

Pokemon::Segment make_story_segment(const std::vector<StoryCheckpoint>& checkpoints, StoryExecutor& executor){
    Pokemon::Segment segment;
    segment.id = "00";
    for (const StoryCheckpoint& data : checkpoints){
        segment.checkpoints.push_back(Pokemon::CheckpointSpec{
            data.id, data.title, false,
            [&data, &executor]{ return build_checkpoint(data, executor); }
        });
    }
    return segment;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
