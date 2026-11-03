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
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecoveryWalk.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryAbilities.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStorySummary.h"
#include "PokemonFRLG_AutoStoryTeachScreens.h"
#include "PokemonFRLG_AutoStoryWinChance.h"

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
    const std::optional<std::vector<RouteStep>> route = fixed_route(story_maps(), start, step.legs);
    if (!facing || !route || route->empty()){
        return Pokemon::outcome_failed("bad_legs", to_string(start));
    }
    const OverworldInterruptSet interrupts = story_interrupts(env.supplies, step.fight_wild, env.checks, env.language);
    //  Planning lets the walker recover a stopped open leg: it plans back to
    //  the leg's end, then walks the rest as given.
    const RoutePlanning planning{.maps = &story_maps(), .costs = default_route_costs(), .start = start, .label = step.name, .wall_check = wall_check(env)};
    walk_overworld_route_with_recovery(env.console, env.context, *route, planning, facing, interrupts, move_speed(step));
    log_arrival(env, step.name, route->back().end.map, route->back().end.point);
    return Pokemon::outcome_ok(to_pose(StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after}));
}

Outcome take_step(StoryEnv& env, RunState& state, const StoryStep& step){
    int tiles_stepped = 1;
    if (step.variant == TRAINER_LINE_VARIANT){
        const std::optional<int> stepped = step_into_line(env.console, env.context, step.direction, facing_of(state.pose), TRAINER_LINE_TILES);
        if (!stepped){
            return Pokemon::outcome_failed("no_trainer", std::format("{} tiles {}", TRAINER_LINE_TILES, direction_name(step.direction)));
        }
        env.console.log(std::format("Story: {}: a trainer saw the player after {} tile(s).", step.name, *stepped));
        tiles_stepped = *stepped;
    }else{
        step_once(env.context, step.direction, facing_of(state.pose));
    }
    const std::vector<TilePos> tiles = Pokemon::tiles_of(state.pose);
    if (tiles.size() != 1){
        return Pokemon::outcome_ok();
    }
    TilePos onto = tiles[0];
    for (int c = 0; c < tiles_stepped; c++){
        onto = onto.next(step.direction);
    }
    return Pokemon::outcome_ok(to_pose(StoryPose{state.pose.map, onto, step.direction}));
}

//  Before a BattleRisk::HIGH battle the story may lose: the sim's win
//  chance, the HP as the last bar read left it. Why not to fight, if so.
std::optional<std::string> refuse_battle(StoryEnv& env, const StoryStep& step, std::string_view trainer){
    const Supplies& supplies = env.supplies;
    if (step.may_lose || supplies.risk != BattleRisk::HIGH){
        return std::nullopt;
    }
    const SimItems items{.potions = supplies.potions, .awakenings = supplies.awakenings};
    const std::optional<WinEstimate> estimate = story_win_chance(supplies.lead, items, supplies.hp_fill, trainer);
    if (!estimate){
        return std::nullopt;
    }
    env.console.log(std::format(
        "Story: {} against {}: the sim wins {} of {} ({:.0f}%) with {}.",
        step.name, trainer, estimate->wins, estimate->battles, 100 * estimate->chance, estimate->inputs
    ));
    return win_chance_refusal(*estimate);
}

Outcome battle(StoryEnv& env, const StoryStep& step){
    const std::optional<std::string_view> trainer = step.pose_after ? story_trainer(step.pose_after->map, step.name) : std::nullopt;
    env.supplies.trainer = trainer ? std::string(*trainer) : "";
    env.supplies.risk = trainer ? story_battle_risk(step.pose_after->map, step.name) : BattleRisk::LOW;
    env.supplies.foe_out.reset();
    if (const std::optional<std::string> refusal = trainer ? refuse_battle(env, step, *trainer) : std::nullopt){
        env.console.log("Story: " + step.name + ": " + *refusal + ".", COLOR_RED);
        return Pokemon::outcome_failed("win_chance", *refusal);
    }
    if (advance_text(env.console, env.context, BATTLE_TEXT_TIMEOUT, true) != TextEnd::BATTLE){
        return Pokemon::outcome_failed("no_battle");
    }
    env.supplies.foes = trainer ? trainer_party(*trainer) : std::vector<std::string>{};
    if (!env.supplies.foes.empty()){
        env.console.log(std::format(
            "Story: {} is {}, risk {}; attacks are chosen against the foe out.",
            step.name, *trainer, battle_risk_name(env.supplies.risk)
        ));
    }
    const BattleEnd end = fight_battle(env.console, env.context, env.supplies, BattleKind::TRAINER, env.checks, env.language);
    env.supplies.foes.clear();
    env.supplies.trainer.clear();
    env.supplies.foe_out.reset();
    if (end == BattleEnd::LOST && !step.may_lose){
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
    for (const std::string& item : set_from_bag(env.supplies, rows)){
        env.console.log(std::format(
            "Story: couldn't read how many {} the bag holds; counting {}.",
            item, item_count(env.supplies, item)
        ), COLOR_ORANGE);
    }
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

MoveSpeed move_speed(const StoryStep& step){
    return step.variant == RUN_VARIANT ? MoveSpeed::RUN : MoveSpeed::WALK;
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
    static const std::pair<std::string, const char*> PLACES[] = {
        {"PalletTown", "pallet-town"},
        {"ViridianForest", "viridian-forest"},
        {"ViridianCity", "viridian-city"},
        {"PewterCity", "pewter-city"},
        {"Route3", "route-3"},
        {"Route4", "route-4"},
        {"CeruleanCity", "cerulean-city"},
    };
    for (const auto& [prefix, place] : PLACES){
        if (map == prefix || map.rfind(prefix + "_", 0) == 0){
            return place;
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

std::optional<Direction> wall_beside(const MapData& map, const TilePos& tile){
    for (Direction direction : {Direction::WEST, Direction::EAST, Direction::NORTH, Direction::SOUTH}){
        const TilePos next = tile.next(direction);
        if (in_bounds(map, next) && tile_at(map, next) == TileKind::WALL && object_at(map, next) == nullptr){
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
    play_new_game_intro(m_env.console, m_env.context, m_env.player_name);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::options(RunState&, const StoryStep&){
    set_options(m_env.console, m_env.context, m_menu, m_env.language);
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
    healed_by_nurse(m_env.supplies);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::shop(RunState&, const StoryStep& step){
    return PokemonFRLG::shop(m_env, step);
}

Outcome StoryExecutor::ready(RunState& state, const StoryStep& step){
    return get_ready(m_env, state, step);
}

namespace{

bool walked(StoryEnv& env, RunState& state, const StoryStep& step){
    const Outcome outcome = walk_route(env, state, step);
    if (outcome.succeeded() && outcome.pose){
        state.pose = *outcome.pose;
    }
    return outcome.succeeded();
}

//  A GRIND's trip from the patch to the nurse (`step`'s target) and back
//  to `patch`, starting where pacing left the player (`spot`).
bool grind_heal_trip(StoryEnv& env, RunState& state, const StoryStep& step, const Pokemon::Pose& patch, const GrindSpot& spot){
    std::optional<StoryPose> here = story_pose(patch);
    if (!here){
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
    walk.target = step.target;
    if (!walked(env, state, walk)){
        return false;
    }
    heal_at_center(env.console, env.context);
    //  PP too: a grind's battles spend more than its attacks have.
    healed_by_nurse(env.supplies);
    walk.name = step.name + "_back_to_patch";
    walk.target = patch;
    return walked(env, state, walk);
}

//  A MART, TEACH, LEVEL, GRIND or fossil step on the console; nothing for
//  another variant.
std::optional<Outcome> run_abilities(StoryEnv& env, RunState& state, const StoryStep& step){
    const Pokemon::Pose patch = state.pose;
    ConsoleMartActions mart(env.console, env.context, env.language, env.supplies);
    ConsoleTeachScreens teach_screens(env.console, env.context, env.language);
    ScreenTeachActions teach(env.console.logger(), teach_screens);
    ConsoleGrindActions grind(
        env.console, env.context, env.language, env.supplies, env.checks,
        [&](const GrindSpot& spot){
            return grind_heal_trip(env, state, step, patch, spot);
        }
    );
    ConsoleChoiceActions choice(env.console, env.context);
    StoryAbilities abilities{console_member_reader(env.console, env.context, env.language), mart, teach, grind, choice};
    return run_ability_game_step(env.console.logger(), &env.console, step, abilities);
}

}

Outcome StoryExecutor::game(RunState& state, const StoryStep& step){
    if (step.variant == CLOSE_TRAINER_CARD){
        close_trainer_card(m_env.console, m_env.context);
        return Pokemon::outcome_ok();
    }
    if (step.variant == LEAVE_GIFT_SUMMARY){
        leave_gift_summary(m_env.console, m_env.context);
        return Pokemon::outcome_ok();
    }
    ConsoleCaptureActions capture(m_env.console, m_env.context, m_env.language);
    ConsolePcStorageActions pc(m_env.console, m_env.context);
    const std::optional<Outcome> stored = run_storage_game_step(m_env.console.logger(), &m_env.console, step, capture, pc);
    if (stored){
        return *stored;
    }
    const std::optional<Outcome> ability = run_abilities(m_env, state, step);
    if (ability){
        return *ability;
    }
    return Pokemon::unsupported_step(step);
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

}

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

Pokemon::Checkpoint build_checkpoint(const StoryCheckpoint& data, StoryExecutor& executor){
    executor.set_menu(data.menu);
    StoryEnv& env = executor.env();
    Pokemon::Checkpoint checkpoint = Pokemon::build_story_checkpoint(data, executor, env.prepared);
    env.start_map = data.start ? data.start->map : "";
    env.start_menu = data.menu;
    const StartMenu menu = data.menu;
    checkpoint.save = [&env, menu](RunState&){
        return guarded([&]{
            save_game(env.console, env.context, menu);
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



}
}
}
