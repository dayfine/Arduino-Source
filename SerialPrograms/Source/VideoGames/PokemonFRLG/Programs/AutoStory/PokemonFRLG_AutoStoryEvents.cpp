/*  AutoStory Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <memory>
#include <set>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/Navigation/Pokemon_RouteLog.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecovery.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryEvents.h"

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
    env.console.log(Pokemon::ROUTE_LOG_TAG + Pokemon::route_arrived_line(label, {{map, tile}}));
}

//  The walker's check that each leg to a wall ended at it (a diagnostic):
//  with Thorough checks only, as Fast trusts the walk.
WallCheck wall_check(const StoryEnv& env){
    return env.checks == StoryChecks::THOROUGH ? WallCheck::TAP : WallCheck::SKIP;
}

Outcome walk_route(StoryEnv& env, RunState& state, const StoryStep& step){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(state.pose);
    if (tiles.size() != 1 || state.pose.certainty != Pokemon::PoseCertainty::TILE){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
    }
    const MapPosition start{state.pose.map, tiles[0]};
    std::optional<Direction> facing = facing_of(state.pose);
    if (!facing){
        facing = face_a_wall(env, start);
    }
    if (!facing){
        return Pokemon::outcome_failed("unknown_facing", to_string(start));
    }
    const std::optional<StoryPose> target = step.target ? story_pose(*step.target) : std::nullopt;
    if (!target){
        return Pokemon::outcome_failed("bad_target", step.target ? Pokemon::to_string(*step.target) : "none");
    }
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        story_maps(), start, goal, default_route_costs(), facing
    );
    if (!route){
        return Pokemon::outcome_failed("no_route", to_string(start) + " to " + to_string(goal.position));
    }
    const OverworldInterruptSet interrupts = story_interrupts(env.supplies, step.fight_wild, env.checks, env.language);
    //  Planning lets the walker recover a stopped open leg: it plans back to
    //  the leg's end, then walks the rest as given.
    const RoutePlanning planning{.maps = &story_maps(), .costs = default_route_costs(), .start = start, .label = step.name, .wall_check = wall_check(env)};
    walk_overworld_route(env.console, env.context, *route, facing, interrupts, &planning);
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
    const std::optional<std::vector<RouteStep>> route = fixed_route(story_maps(), start, step.legs);
    if (!facing || !route || route->empty()){
        return Pokemon::outcome_failed("bad_legs", to_string(start));
    }
    const OverworldInterruptSet interrupts = story_interrupts(env.supplies, step.fight_wild, env.checks, env.language);
    //  Planning lets the walker recover a stopped open leg: it plans back to
    //  the leg's end, then walks the rest as given.
    const RoutePlanning planning{.maps = &story_maps(), .costs = default_route_costs(), .start = start, .label = step.name, .wall_check = wall_check(env)};
    walk_overworld_route(env.console, env.context, *route, facing, interrupts, &planning);
    log_arrival(env, step.name, route->back().end.map, route->back().end.point);
    return Pokemon::outcome_ok(to_pose(StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after}));
}

Outcome take_step(StoryEnv& env, RunState& state, const StoryStep& step){
    step_once(env.context, step.direction, facing_of(state.pose));
    const std::vector<TilePos> tiles = Pokemon::tiles_of(state.pose);
    if (tiles.size() != 1){
        return Pokemon::outcome_ok();
    }
    return Pokemon::outcome_ok(to_pose(StoryPose{state.pose.map, tiles[0].next(step.direction), step.direction}));
}

Outcome battle(StoryEnv& env, const StoryStep& step){
    if (advance_text(env.console, env.context, BATTLE_TEXT_TIMEOUT, true) != TextEnd::BATTLE){
        return Pokemon::outcome_failed("no_battle");
    }
    const BattleEnd end = fight_battle(env.console, env.context, env.supplies, BattleKind::TRAINER, env.checks, env.language);
    if (end == BattleEnd::LOST && !step.may_lose){
        return Pokemon::outcome_failed("blackout");
    }
    mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
    return Pokemon::outcome_ok();
}

//  HP and status are the battle loop's last reading. Items first (an
//  Antidote, a Parlyz Heal, a Potion); the nurse only for red HP with no
//  Potion left, or a status no item here cures.
Outcome get_ready(StoryEnv& env, RunState& state, const StoryStep& step){
    Supplies& supplies = env.supplies;
    env.console.log(std::format(
        "Story: before the battle, HP {}, {}, {} Potions.",
        supplies.hp_red ? "red" : supplies.hurt ? "yellow" : "green", status_name(supplies.status), supplies.potions
    ));
    recover_after_battle(env.console, env.context, supplies, env.checks, env.language);
    if (!supplies.hp_red && supplies.status == StatusCondition::NONE){
        return Pokemon::outcome_ok();
    }
    Outcome walked = walk_route(env, state, step);
    if (!walked.succeeded()){
        return walked;
    }
    heal_at_center(env.console, env.context);
    supplies.hurt = false;
    supplies.hp_red = false;
    supplies.status = StatusCondition::NONE;
    return walked;
}

//  Viridian Mart before the Boulder Badge (pokefirered ViridianCity_Mart_Items):
//  POKE BALL, POTION (300), ANTIDOTE (100), PARLYZ HEAL (200). Each item
//  the story buys: its row, and where the bag's count is kept.
struct MartItem{
    const char* item;
    int row;
    int Supplies::* count;
    //  The name the item list reader reads (read_item_list_name()).
    const char* slug;
};
constexpr MartItem VIRIDIAN_MART_ITEMS[] = {
    {.item = "potion", .row = 1, .count = &Supplies::potions, .slug = "potion"},
    {.item = "antidote", .row = 2, .count = &Supplies::antidotes, .slug = "antidote"},
    {.item = "parlyz_heal", .row = 3, .count = &Supplies::parlyz_heals, .slug = "parlyz-heal"},
};

const MartItem* viridian_mart_item(const std::string& name){
    for (const MartItem& item : VIRIDIAN_MART_ITEMS){
        if (name == item.item){
            return &item;
        }
    }
    return nullptr;
}

Outcome shop(StoryEnv& env, const StoryStep& step){
    const std::optional<std::vector<MartOrder>> orders = viridian_mart_orders(step.items);
    if (!orders){
        return Pokemon::outcome_failed("unknown_item", step.name);
    }
    //  2000 of the 3000 a new game starts with, plus the rival's prize.
    buy_items(env.console, env.context, *orders, env.checks, env.language);
    for (const Pokemon::StoryItem& item : step.items){
        env.supplies.*viridian_mart_item(item.item)->count += (int)item.quantity;
    }
    if (env.checks == StoryChecks::FAST){
        return Pokemon::outcome_ok();
    }
    //  The counts from the bag, not from the orders: a wrong row once bought
    //  Parlyz Heals for Antidotes unnoticed (console B, 2026-11-01).
    const std::vector<BagRow> rows = read_items_pocket(env.console, env.context, env.language);
    if (rows.empty()){
        env.console.log("Story: couldn't read the Items pocket; keeping the counts ordered.", COLOR_ORANGE);
        return Pokemon::outcome_ok();
    }
    set_from_bag(env.supplies, rows);
    return Pokemon::outcome_ok();
}

}


std::optional<std::vector<MartOrder>> viridian_mart_orders(const std::vector<Pokemon::StoryItem>& items){
    std::vector<MartOrder> orders;
    for (const Pokemon::StoryItem& wanted : items){
        const MartItem* found = viridian_mart_item(wanted.item);
        if (found == nullptr){
            return std::nullopt;
        }
        orders.push_back(MartOrder{found->row, (int)wanted.quantity, found->slug});
    }
    return orders;
}

const std::vector<MapData>& story_maps(){
    static const std::vector<MapData> maps = overworld_maps(map_flag_set(overworld_maps(), "after_hall_of_fame"));
    return maps;
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
    //  The longer names first: "ViridianForest" before "ViridianCity".
    static const std::pair<const char*, const char*> PLACES[] = {
        {"PalletTown", "pallet-town"},
        {"ViridianForest", "viridian-forest"},
        {"ViridianCity", "viridian-city"},
        {"PewterCity", "pewter-city"},
    };
    for (const auto& [prefix, place] : PLACES){
        if (map.rfind(prefix, 0) == 0){
            return place;
        }
    }
    return "";
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

std::optional<Direction> wall_beside(const MapData& map, const TilePos& tile){
    for (Direction direction : {Direction::WEST, Direction::EAST, Direction::NORTH, Direction::SOUTH}){
        const TilePos next = tile.next(direction);
        if (in_bounds(map, next) && tile_at(map, next) == Tile::WALL && object_at(map, next) == nullptr){
            return direction;
        }
    }
    return std::nullopt;
}

StoryExecutor::StoryExecutor(StoryEnv& env)
    : m_env(env)
{}

Outcome StoryExecutor::run(RunState& state, const StoryStep& step){
    return guarded([&]{ return Pokemon::StoryStepExecutor::run(state, step); });
}

Outcome StoryExecutor::new_game(RunState&, const StoryStep&){
    play_new_game_intro(m_env.console, m_env.context);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::options(RunState&, const StoryStep&){
    set_options(m_env.console, m_env.context, m_menu);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::route(RunState& state, const StoryStep& step){
    return walk_route(m_env, state, step);
}

Outcome StoryExecutor::legs(RunState& state, const StoryStep& step){
    return walk_legs(m_env, state, step);
}

Outcome StoryExecutor::step(RunState& state, const StoryStep& step){
    return take_step(m_env, state, step);
}

Outcome StoryExecutor::talk(RunState&, const StoryStep&){
    pbf_press_button(m_env.context, BUTTON_A, 160ms, 800ms);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::answer(RunState&, const StoryStep& step){
    answer_yes_no(m_env.console, m_env.context, step.yes, BUTTON_B, YES_NO_TIMEOUT);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::mash(RunState&, const StoryStep&){
    mash_until_overworld(m_env.console, m_env.context, SCENE_TIMEOUT);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::battle(RunState&, const StoryStep& step){
    return PokemonFRLG::battle(m_env, step);
}

Outcome StoryExecutor::heal(RunState&, const StoryStep&){
    heal_at_center(m_env.console, m_env.context);
    m_env.supplies.hurt = false;
    m_env.supplies.hp_red = false;
    m_env.supplies.status = StatusCondition::NONE;
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::shop(RunState&, const StoryStep& step){
    return PokemonFRLG::shop(m_env, step);
}

Outcome StoryExecutor::ready(RunState& state, const StoryStep& step){
    return get_ready(m_env, state, step);
}


Pokemon::Checkpoint build_checkpoint(const StoryCheckpoint& data, StoryExecutor& executor){
    executor.set_menu(data.menu);
    Pokemon::Checkpoint checkpoint = Pokemon::build_story_checkpoint(data, executor);
    StoryEnv& env = executor.env();
    const StartMenu menu = data.menu;
    checkpoint.save = [&env, menu](RunState&){
        return guarded([&]{
            save_game(env.console, env.context, menu);
            env.has_save = true;
            env.saved_supplies = env.supplies;
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



}
}
}
