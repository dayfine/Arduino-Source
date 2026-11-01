/*  BDSP AutoStory - Events: Sandgem and Route 202 (00.04)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  00.04's Potions in Sandgem (the Poke Mart lady, or the clerk) and the
 *  way back to Route 202's save spot, and the stairs into Jubilife.
 *
 */

#include <algorithm>
#include <cmath>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Bag.h"
#include "PokemonBDSP_AutoStory_Center.h"
#include "PokemonBDSP_AutoStory_Mart.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::TilePos;


namespace{

// How many Potions to buy. A Potion is 300 and a new game starts with
// 3000; the money on screen cannot be read yet. Five heal the starter
// through Route 202's four trainers and leave half the money.
constexpr int POTIONS_TO_BUY = 5;

// From below the Center's door to below the Poke Mart's, ten tiles
// east along the same row, and in. The last leg is open and movement
// is not locked to tiles, so we may stand part of a tile off the door:
// whole-tile steps either side missed it in r121a. The Mart's doorway
// is lit like the Center's, so it is lined up with by sight the same
// way. Two townspeople wander the dirt south of the Mart and stop a
// leg short without the walk knowing (r122a counted to the door's
// row from three rows south of it): where the doorway shows on screen
// says so, and the walk is made once more from there.
void enter_sandgem_poke_mart(
    VideoStream& stream, ProControllerContext& context, const StoryWorld& maps,
    std::vector<TrainerSight>& trainers, const RouteTrainerFight& fight
) {
    const TilePos door = maps.to_world(MAP_SANDGEM_TOWN, map_point("SandgemTown", "poke_mart_door"));
    const TilePos front = door.next(Pokemon::Direction::SOUTH);
    const auto at_front = [&](const TilePos& pos) { return pos.y == front.y && std::abs(pos.x - front.x) <= 1; };
    walk_route(stream, context, run_from_wild_battle, maps, story_door_front(MAP_SANDGEM_TOWN, map_point("SandgemTown", "pokemon_center_door")),
        at_front, trainers, fight, "To the Poke Mart");
    const std::vector<TilePos> short_of_door = tiles_short_of_door(stream, context, maps, door, "To the Poke Mart");
    if (!short_of_door.empty()) {
        route_status(stream, "To the Poke Mart: stopped short of the door; walking again.");
        walk_route(stream, context, run_from_wild_battle, maps, short_of_door, at_front, trainers, fight, "To the Poke Mart again");
    }
    enter_lit_door(stream, context, "Sandgem Poke Mart: through the door");
}

// Inside the Mart, the clerk's counter is straight ahead of the door
// (every Mart is laid out so; Oreburgh's and Eterna's are walked the
// same way). Walk up to it, buy the Potions, and walk back out.
// buy_items() opens the talk and finds the Potion row by OCR of its
// name and price; the quantity is counted out blind.
void buy_potions_at_the_counter(VideoStream& stream, ProControllerContext& context) {
    route_status(stream, "Poke Mart: to the clerk.");
    const WalkResult walk = walk_until(stream, context, run_from_wild_battle, 0, 1, 6000ms, "Poke Mart: to the clerk");
    if (walk.stop != WalkStop::BLOCKED && walk.stop != WalkStop::DIALOG) {
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Poke Mart: the counter was not reached (" + std::string(walk_stop_name(walk.stop)) + ").", stream);
    }
    route_status(stream, "Poke Mart: buying " + std::to_string(POTIONS_TO_BUY) + " Potions.");
    if (!buy_items(stream, context, {{ShopItem::Potion, POTIONS_TO_BUY}})) {
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Poke Mart: could not buy " + std::to_string(POTIONS_TO_BUY) + " Potions.", stream);
    }
    leave_door_south(stream, context, run_from_wild_battle, "Poke Mart: out the door");
}

// Potions let us heal where we stand. Sandgem's Poke Mart is next
// door east of its Center. The purchase is checked in the Bag after,
// since buy_items() counts the quantity out without reading it.
// Returns where we stand after: below the Mart's door.
std::vector<TilePos> buy_potions_in_sandgem(
    VideoStream& stream, ProControllerContext& context, const StoryWorld& maps,
    std::vector<TrainerSight>& trainers, const RouteTrainerFight& fight
) {
    enter_sandgem_poke_mart(stream, context, maps, trainers, fight);
    buy_potions_at_the_counter(stream, context);
    if (!bag_has_item(stream, context, "potion", 1)) {
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Poke Mart: the purchase went through, but no Potion is in the Bag.", stream);
    }
    return story_door_front(MAP_SANDGEM_TOWN, map_point("SandgemTown", "poke_mart_door"));
}

// A walk_until() callback that keeps every location banner it sees.
std::function<bool(const ImageViewRGB32&)> banner_keeper(std::vector<std::string>& banners) {
    return [&banners](const ImageViewRGB32& screen) {
        const std::string read = read_location_banner(screen);
        if (!read.empty() && (banners.empty() || banners.back() != read)) {
            banners.push_back(read);
        }
        return false;
    };
}

bool any_banner_names(const std::vector<std::string>& banners, const char* map) {
    return std::any_of(banners.begin(), banners.end(),
        [&](const std::string& read) { return location_banner_matches(read, map); });
}

// Whether all of `tiles` are on `map`.
bool all_on_map(const StoryWorld& maps, const std::vector<TilePos>& tiles, const char* map) {
    return !tiles.empty() && std::all_of(tiles.begin(), tiles.end(), [&](const TilePos& pos) {
        const std::optional<MapTile> tile = maps.to_local(pos);
        return tile && tile->map == map;
    });
}

// North from below the gap between the signs until a wall stops us,
// nudging a tile west, twice at most, when blocked at once (below a
// sign or the Mart). Returns whether Route 202's banner showed: only
// one of the gap's columns leads out of town before a wall.
bool walk_north_through_gap(VideoStream& stream, ProControllerContext& context) {
    using Pokemon::Direction;
    std::vector<std::string> banners;
    for (size_t nudges = 0;; nudges++){
        const WalkResult north = walk_until(stream, context, run_from_wild_battle, 0, 1, 8000ms, "North through the gap", banner_keeper(banners));
        if (north.stop == WalkStop::TRANSITION || north.stop == WalkStop::DIALOG || north.wild_battles > 0){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "North through the gap: " + std::string(walk_stop_name(north.stop)) + " instead of a wall.", stream);
        }
        //  Blocked at once: the wall reads after about 800ms still.
        if (north.walked > ROUTE_START_DELAY + 800ms + 2 * ROUTE_MS_PER_TILE_UP_DOWN){
            break;
        }
        if (nudges == 2){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "North through the gap: still blocked after two tiles west.", stream);
        }
        route_status(stream, "North through the gap: blocked at once, below a sign or the Mart; a tile west.");
        route_turn(stream, context, Direction::WEST);
        walk_for(stream, context, run_from_wild_battle, -1, 0, route_open_hold(Direction::WEST, 1), "A tile west");
        route_turn(stream, context, Direction::NORTH);
    }
    context.wait_for_all_requests();
    if (VideoSnapshot screen = stream.video().snapshot()){
        banner_keeper(banners)(*screen.frame);
    }
    return any_banner_names(banners, MAP_ROUTE_202);
}

// From the trees at the east end of the doors' row, west into the gap
// between the Center's sign and the Mart's (below_sign_gap()) and
// north through it until a wall stops us. Six tiles west from column
// 29 ends on 23, and on 20 to 26 if two tiles off: north from all of
// those passes no door, and only the gap lets us through. A walk
// north that goes nowhere stood below a sign or the Mart: a tile
// west, twice at most, never reaches the Center's door at 17. The
// planner would rather go round the south of town on walls, where
// townspeople wander. Returns where the walk north may have ended:
// the gap's columns end on Sandgem's top row, bar one that runs on to
// Route 202, and its banner tells the two apart.
std::vector<TilePos> north_through_sign_gap(VideoStream& stream, ProControllerContext& context, const StoryWorld& maps){
    using Pokemon::Direction;
    route_status(stream, "To the gap between the Center's and the Mart's signs: west 6.");
    route_turn(stream, context, Direction::WEST);
    walk_for(stream, context, run_from_wild_battle, -1, 0, route_open_hold(Direction::WEST, 6), "To the gap between the signs");
    const bool on_route = walk_north_through_gap(stream, context);
    std::vector<TilePos> ends;
    for (const TilePos& pos : map_region("SandgemTown", "below_sign_gap")){
        const TilePos end = slide_to_wall(maps, maps.to_world(MAP_SANDGEM_TOWN, pos), Direction::NORTH).stop;
        if (all_on_map(maps, {end}, MAP_ROUTE_202) == on_route){
            ends.push_back(end);
        }
    }
    route_status(stream, std::string("North through the gap: Route 202's banner ") +
        (on_route ? "showed; on Route 202." : "did not show; on Sandgem Town's top row."));
    return ends;
}

// Back up to Route 202's save spot under the ledge: it is where a
// retry may start (story_save_spots()), its walls pin it, and the
// trainers are north of it. First east to the trees that end the
// doors' row, so the way north starts from one known tile, then north
// through the gap between the signs: r123a, planned round the south
// of town on a map that had it wrong, walked into a house.
void walk_to_route_202_save_spot(
    VideoStream& stream, ProControllerContext& context, const StoryWorld& maps,
    std::vector<TilePos> at, std::vector<TrainerSight>& trainers, const RouteTrainerFight& fight
) {
    const TilePos row_end = maps.to_world(MAP_SANDGEM_TOWN, map_point("SandgemTown", "front_row_east_end"));
    const TilePos save_spot = maps.to_world(MAP_ROUTE_202, map_point("Route202", "save_spot"));
    if (!all_on_map(maps, at, MAP_ROUTE_202)) {
        at = walk_route(stream, context, run_from_wild_battle, maps, std::move(at),
            [&](const TilePos& pos) { return pos == row_end; }, trainers, fight, "To the trees east of the Poke Mart");
        at = north_through_sign_gap(stream, context, maps);
    }
    walk_route(stream, context, run_from_wild_battle, maps, std::move(at),
        [&](const TilePos& pos) { return pos == save_spot; }, trainers, fight, "Back to Route 202");
}

// North into Jubilife until Dawn's scene starts (00.04d's
// into_jubilife). Jubilife's south edge is a wall with stairs in it,
// west of where the route may leave us (r78 stood at its east end).
// Entering starts a scene: Dawn walks over while we stand still. So
// walk north; if the wall stops us and no dialogue follows, the stairs
// are further west. The scene itself is the next step.
void up_jubilife_stairs(VideoStream& stream, ProControllerContext& context) {
    for (size_t attempt = 0; attempt < 6; attempt++) {
        const WalkResult walk = walk_until(stream, context, run_from_wild_battle, 0, 1, 4000ms, "to Dawn in Jubilife");
        if (walk.stop == WalkStop::TRANSITION) {
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Jubilife: left the area instead of meeting Dawn.", stream);
        }
        ShortDialogWatcher dialog(COLOR_CYAN);
        if (walk.stop == WalkStop::DIALOG || wait_until(stream, context, 6s, {{dialog}}) == 0) {
            return;
        }
        stream.log("[00.04] Jubilife: the wall stopped us; looking for the stairs one tile west.");
        route_turn(stream, context, Pokemon::Direction::WEST);
        pbf_move_left_joystick(context, {-1, 0}, route_open_hold(Pokemon::Direction::WEST, 1), 200ms);
        context.wait_for_all_requests();
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        "Jubilife: no stairs found and no Dawn after six tiles west.", stream);
}

//  Reads a sign facing `facing`: its box must open and close, and its text,
//  when there is a reader for it, have `key`. A walk that stopped short of
//  it is put right by walking on until the sign stops us, once.
void read_sign_on_the_way(VideoStream& stream, ProControllerContext& context, Direction facing, const std::string& sign, const std::string& key){
    for (size_t attempt = 0; attempt < 2; attempt++){
        const DialogueRead read = read_sign(stream, context, facing, sign);
        if (read.opened && read.closed && (read.text.empty() || sign_text_matches(read.text, key))){
            return;
        }
        walk_until_stopped(stream, context, facing, 2, "On to the sign");
    }
    fail_step(stream, "Sandgem: " + sign + " did not read facing " + Pokemon::direction_name(facing) + ".");
}

//  From the doors' row in front of Sandgem's Pokemon Center to the Poke Mart
//  lady and back to Route 202's save spot. Every leg but three one-tile
//  steps walks until something stops it, as measured on the console
//  (2026-10-30, CFR "walk"): west to the Lab's sign; south a tile, west to
//  the grass's edge; north to the trees, on her row; east until she stops
//  us (she gives new games 10 Potions); south a tile, east to Route 201's
//  sign; south a tile, east to the trees east of the Mart, north a row; then
//  the planned walk to the save spot. She stands on the top row of her
//  grass, a tile or two in from its west edge: missed, back west to the
//  edge and again, twice.
void mart_lady(VideoStream& stream, ProControllerContext& context){
    walk_until_stopped(stream, context, Direction::WEST, 20, "West to the Lab's sign");
    read_sign_on_the_way(stream, context, Direction::WEST, "the Lab's sign", "lab");
    hold_for(stream, context, Direction::SOUTH, 200ms, "South onto the path");
    walk_until_stopped(stream, context, Direction::WEST, 28, "West along the path");
    walk_until_stopped(stream, context, Direction::NORTH, 3, "North to the trees");
    bool talked = false;
    for (size_t attempt = 0; attempt < 3 && !talked; attempt++){
        if (attempt > 0){
            walk_until_stopped(stream, context, Direction::WEST, 6, "West to the grass's edge");
        }
        const DialogueRead read = talk_walking(stream, context, Direction::EAST, 4, "Poke Mart lady");
        talked = read.opened && read.closed;
    }
    if (!talked){
        fail_step(stream, "Route 201: the Poke Mart lady did not talk.");
    }
    hold_for(stream, context, Direction::SOUTH, 200ms, "South onto the signs' row");
    walk_until_stopped(stream, context, Direction::EAST, 16, "East to the Route 201 sign");
    read_sign_on_the_way(stream, context, Direction::EAST, "Route 201's sign", "201");
    hold_for(stream, context, Direction::SOUTH, 200ms, "South onto the path");
    walk_until_stopped(stream, context, Direction::EAST, 40, "East along the path");
    walk_until_stopped(stream, context, Direction::NORTH, 2, "North to the front row");
    const StoryWorld& maps = story_maps();
    std::vector<TrainerSight> trainers = story_trainer_sights();
    const RouteTrainerFight fight = [&](const std::string& name){ fight_route_trainer(stream, context, name); };
    walk_to_route_202_save_spot(stream, context, maps, {maps.to_world(MAP_SANDGEM_TOWN, map_point("SandgemTown", "front_row_east_end"))}, trainers, fight);
}

//  00.04c's potions step, from the save out of Sandgem's Center: the Poke
//  Mart lady's, or the clerk's after two misses this run, on to Route 202's
//  save spot, where 00.04d's save goes. The Bag must have Potions after.
//  The clerk's list is found by reading item names, which needs a reader
//  for the game's language; without one, the lady every time.
Pokemon::Outcome sandgem_potions(StoryExecutor& executor, Pokemon::RunState&, const Pokemon::StoryStep&){
    VideoStream& stream = executor.stream();
    ProControllerContext& context = executor.context();
    size_t& misses = executor.run_count("mart_lady_misses");
    if (misses < 2 || !story_reads_text()){
        // Onto the doors' row first: out of the door we stand a tile to
        // three below it (r261 walked west into the trees from three).
        route_turn(stream, context, Direction::EAST);
        walk_tiles_dpad(stream, context, run_from_wild_battle, 1, 0, 1, "east under the Center's wall");
        const WalkResult up = walk_until(stream, context, run_from_wild_battle, 0, 1, 3000ms, "north to the Center's front");
        if (up.stop != WalkStop::BLOCKED){
            fail_step(stream, std::string("Sandgem: north to the Center's front ended ") + walk_stop_name(up.stop) + ".");
        }
        try{
            mart_lady(stream, context);
        }catch (OperationFailedException&){
            misses++;
            throw;
        }
    }else{
        const StoryWorld& maps = story_maps();
        std::vector<TrainerSight> trainers = story_trainer_sights();
        const RouteTrainerFight fight = [&](const std::string& name){ fight_route_trainer(stream, context, name); };
        const std::vector<TilePos> at = buy_potions_in_sandgem(stream, context, maps, trainers, fight);
        walk_to_route_202_save_spot(stream, context, maps, at, trainers, fight);
    }
    if (!bag_has_item(stream, context, "potion", 1)){
        fail_step(stream, "00.04: no Potions in the Bag after the shop.");
    }
    return Pokemon::outcome_ok();
}

}


void add_sandgem_events(StoryEvents& events){
    events.emplace("sandgem_potions", sandgem_potions);
    events.emplace("jubilife_stairs", console_event(up_jubilife_stairs));
}



}
}
}
