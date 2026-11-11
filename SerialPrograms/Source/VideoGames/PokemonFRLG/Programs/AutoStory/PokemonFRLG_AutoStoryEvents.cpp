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
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStorySummary.h"
#include "PokemonFRLG_AutoStoryTable.h"

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
    if (!checkpoint.start){
        return std::nullopt;
    }
    return Pokemon::pose_of(*checkpoint.start);
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
    const Pokemon::Pose target_pose = Pokemon::pose_of(arg<Pokemon::GridPose>(step, "target"));
    const std::optional<StoryPose> target = story_pose(target_pose);
    if (!target){
        return Pokemon::outcome_failed("bad_target", Pokemon::to_string(target_pose));
    }
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        story_maps(), start, goal, default_route_costs(), facing
    );
    if (!route){
        return Pokemon::outcome_failed("no_route", to_string(start) + " to " + to_string(goal.position));
    }
    const OverworldInterruptSet interrupts = story_interrupts(env.supplies, arg<bool>(step, "fight_wild"), env.checks, env.language);
    walk_overworld_route(env.console, env.context, *route, facing, interrupts, move_speed(step));
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
    const OverworldInterruptSet interrupts = story_interrupts(env.supplies, arg<bool>(step, "fight_wild"), env.checks, env.language);
    walk_overworld_route(env.console, env.context, *route, facing, interrupts, move_speed(step));
    log_arrival(env, step.name, route->back().end.map, route->back().end.point);
    return Pokemon::outcome_ok(to_pose(StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after}));
}

Outcome take_step(StoryEnv& env, RunState& state, const StoryStep& step){
    const Direction direction = arg<Direction>(step, "direction");
    step_once(env.context, direction, facing_of(state.pose));
    const std::vector<TilePos> tiles = Pokemon::tiles_of(state.pose);
    if (tiles.size() != 1){
        return Pokemon::outcome_ok();
    }
    return Pokemon::outcome_ok(to_pose(StoryPose{state.pose.map, tiles[0].next(direction), direction}));
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

//  Viridian Mart before the Boulder Badge (pokefirered ViridianCity_Mart_Items):
//  POKE BALL, POTION (300), ANTIDOTE (100), PARLYZ HEAL (200). Each item
//  the story buys: its row, and where the bag's count is kept.
struct MartItem{
    std::string_view item;
    int row;
    int Supplies::* count;
    //  The name the item list reader reads (read_item_list_name()).
    std::string_view slug;
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
    const std::optional<std::vector<MartOrder>> orders = viridian_mart_orders(arg<std::vector<Pokemon::StoryItem>>(step, "items"));
    if (!orders){
        return Pokemon::outcome_failed("unknown_item", step.name);
    }
    //  2000 of the 3000 a new game starts with, plus the rival's prize.
    buy_items(env.console, env.context, *orders, env.checks, env.language);
    for (const Pokemon::StoryItem& item : arg<std::vector<Pokemon::StoryItem>>(step, "items")){
        env.supplies.*viridian_mart_item(item.item)->count += (int)item.quantity;
    }
    if (!checks_screen(env.checks)){
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

} // namespace


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
    //  The longer names first: "ViridianForest" before "ViridianCity".
    static const std::pair<std::string_view, std::string_view> PLACES[] = {
        {"PalletTown", "pallet-town"},
        {"ViridianForest", "viridian-forest"},
        {"ViridianCity", "viridian-city"},
        {"PewterCity", "pewter-city"},
    };
    for (const auto& [prefix, place] : PLACES){
        if (map.rfind(prefix, 0) == 0){
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

namespace{

//  How the console does an action that needs only the env.
using ConsoleAction = Outcome (*)(StoryEnv& env, RunState& state, const StoryStep& step);

ConsoleAction console_action_of(StoryActionId id){
    switch (id){
    case StoryActionId::NEW_GAME:
        return [](StoryEnv& env, RunState&, const StoryStep&){
            play_new_game_intro(env.console, env.context, env.player_gender);
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
        return [](StoryEnv& env, RunState&, const StoryStep& step){ return shop(env, step); };
    case StoryActionId::READY:
        return get_ready;
    case StoryActionId::OPTIONS:
    case StoryActionId::GAME:
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
    if (id == StoryActionId::GAME){
        return {.act = [](const StoryStep& step, RunState&){ return Pokemon::unsupported_step(step); }, .needs_console = true};
    }
    const ConsoleAction action = console_action_of(id);
    return {.act = [this, action](const StoryStep& step, RunState& state){ return action(m_env, state, step); }, .needs_console = true};
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
    for (const std::string& item : set_from_loaded_bag(env.supplies, env.saved_supplies, rows)){
        env.console.log(std::format(
            "Story: couldn't read how many {} the bag holds; counting {}.", item, item_count(env.supplies, item)
        ), COLOR_ORANGE);
    }
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
    Pokemon::Checkpoint checkpoint = Pokemon::build_story_checkpoint(data, executor);
    add_start_check(
        checkpoint, start_pose(data),
        [&env, id = data.id]{ return read_console_start(env, id); },
        [&env](const std::string& line){ env.console.log(line, COLOR_ORANGE); }
    );
    env.start_map = data.start ? Pokemon::grid_pose_of(*data.start).map : "";
    env.start_menu = data.menu;
    const StartMenu menu = data.menu;
    checkpoint.save = [&env, menu, id = data.id](RunState&){
        return guarded([&]{
            save_game(env.console, env.context, menu);
            keep_next_start_frame(env, id);
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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
