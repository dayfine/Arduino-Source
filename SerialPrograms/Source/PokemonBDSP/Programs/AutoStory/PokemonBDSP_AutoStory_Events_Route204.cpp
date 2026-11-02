/*  BDSP AutoStory - Events: Route 204 to Floaroma (01.08 to 01.11)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Segment 01's walks up Route 204 South and through the Ravaged Path, up Route 204 North into
 *  Floaroma, and to the old couple in its flower field, as they ran on the
 *  console (2026-10-31).
 *
 */

#include <algorithm>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_SelectionArrow.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

//  "This rock seems breakable with a hidden move. Would you like to use Rock
//  Smash?" is open, from an A at a smashable rock. Its Yes/No box comes only
//  once the text is advanced again (by hand, 2026-10-31: the text complete
//  1.2 s after the first A, the box 1.2 s after a second), and r237a's timed
//  A, A, B put its B on that box: No. So press A until the box's cursor
//  shows, then once more for Yes, and wait out the move. False if no box
//  shows: whatever is open is not this prompt.
bool answer_rock_smash_prompt(VideoStream& stream, ProControllerContext& context, const std::string& label){
    static const ImageFloatBox YES_NO_ARROW_BOX{0.700, 0.620, 0.070, 0.120};
    SelectionArrowFinder yes_no(stream.overlay(), YES_NO_ARROW_BOX, COLOR_GREEN);
    for (size_t press = 0; press < 5; press++){
        context.wait_for(1200ms);
        context.wait_for_all_requests();
        const VideoSnapshot screen = stream.video().snapshot();
        if (screen && yes_no.detect(*screen.frame)){
            stream.log("[Route] " + label + ": Rock Smash offered; Yes.");
            pbf_press_button(context, BUTTON_A, 80ms, 0ms);
            context.wait_for(5000ms);
            if (dialogue_opens(stream, context, 500ms)){
                finish_dialogue(stream, context, BUTTON_B, label + ": after Rock Smash");
            }
            return true;
        }
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
    }
    stream.log("[Route] " + label + ": no Yes/No box for Rock Smash.", COLOR_RED);
    return false;
}

//  01.08: north out of Jubilife onto Route 204.
void north_out_of_jubilife(VideoStream& stream, ProControllerContext& context){
    // The street's column 14 carries on as the route's; the location banner
    // names Route 204 as we cross. A D-pad walk keeps the column; it may
    // stop a tile or two past the border, which the entry set allows for.
    const WalkResult out = walk_until_dpad(stream, context, run_from_wild_battle, {0, 1}, 8000ms, "north out of Jubilife", {.arrived = location_banner_showing});
    if (out.stop != WalkStop::ARRIVED){
        fail_step(stream, "Jubilife: no Route 204 banner walking north up the street.");
    }
    context.wait_for(1500ms);
}

//  01.08: Route 204 South on its grid to below the Ravaged Path's door.
void route_204_south_to_the_door(VideoStream& stream, ProControllerContext& context){
    // On the grid decoded from the game's data (Maps/Route204South.json):
    // a tile west onto the tall grass column that runs up to the cliff,
    // north up it, then west along the cliff to the signboard beside the
    // door (the offline plan: 3 legs). Other trainers' sight lines are kept
    // out of; town_plan_costs() lets the one-tile step land exactly. Lass Sarah's east line crosses
    // the street, so she may stop us for a battle on the way; a Youngster
    // who is not a trainer wanders in front of the door, and the walker
    // waits for him.
    {
        RouteTrainers trainers{route204_south_trainer_sights(), [&](const std::string& who){ talk_or_fight(stream, context, who); }};
        const std::vector<Pokemon::TilePos> at = walk_route(stream, context, run_from_wild_battle, route204_south_maps(),
            map_region("Route204South", "jubilife_entry"), trainers, RouteWalkPlan{[](const Pokemon::TilePos& pos){ return pos == map_point("Route204South", "below_door"); }, "Route 204 South to the Ravaged Path", route204_south_tiles_to_avoid(), town_plan_costs()});
        if (at.size() != 1 || !(at[0] == map_point("Route204South", "below_door"))){
            fail_step(stream, "Route 204: the walk did not end below the Ravaged Path's door.");
        }
    }
}

//  01.08: north through the door.
void into_the_ravaged_path(VideoStream& stream, ProControllerContext& context){
    if (!dpad_through_door_north(stream, context, "Ravaged Path door")){
        fail_step(stream, "Route 204: no dark screen walking north into the Ravaged Path.");
    }
    context.wait_for(2000ms);
}

//  01.09: through the Ravaged Path, smashing its rocks.
void through_the_ravaged_path(VideoStream& stream, ProControllerContext& context){
    // On the traced grid (Maps/RavagedPath.json): from the
    // south door up the stairs to the tile west of one of the two rocks
    // across the lower corridor, smash it, then east and south to the
    // corridor's east end; the way out is a tile west of it, south.
    {
        //  Blocked short of a wall in here, it is a rock the plan counted on
        //  being gone (r237a: the walker took one for a person and said No).
        auto in_the_way = [&](const std::string& who){
            if (!answer_rock_smash_prompt(stream, context, who)){
                talk_or_fight(stream, context, who);
            }
        };
        RouteTrainers no_trainers{{}, in_the_way};
        std::vector<Pokemon::TilePos> at{map_point("RavagedPath", "entrance")};
        const StoryWorld rocks_in = ravaged_path_maps();
        at = walk_route(stream, context, run_from_wild_battle, rocks_in, at, no_trainers, RouteWalkPlan{[&](const Pokemon::TilePos& pos){
                const auto& before = map_region("RavagedPath", "before_rocks");
                return std::find(before.begin(), before.end(), pos) != before.end();
            }, "To a rock across the corridor", {}, route_plan_costs()});
        route_turn(stream, context, Direction::EAST);
        if (!talk_with_nudges(stream, context, run_from_wild_battle, Direction::EAST, "the rock east of us", 2000ms)){
            fail_step(stream, "Ravaged Path: nothing answered facing the rock east of us.");
        }
        if (!answer_rock_smash_prompt(stream, context, "the rock east of us")){
            fail_step(stream, "Ravaged Path: no Rock Smash offer facing the rock east of us.");
        }
        // We may be on either row; the rock east of the other stays, but no
        // leg from our row crosses it.
        std::vector<Pokemon::TilePos> smashed;
        for (const Pokemon::TilePos& pos : map_region("RavagedPath", "before_rocks")){
            smashed.push_back(pos.next(Direction::EAST));
        }
        const StoryWorld rocks_out = ravaged_path_maps(smashed);
        at = walk_route(stream, context, run_from_wild_battle, rocks_out, at, no_trainers, RouteWalkPlan{[&](const Pokemon::TilePos& pos){ return pos == map_point("RavagedPath", "east_end"); }, "To the corridor's east end", {}, route_plan_costs()});
        dpad_tiles(stream, context, DPAD_LEFT, 1, "west onto the tile above the way out");
        const WalkResult out = walk_dir(stream, context, Direction::SOUTH, 4000ms, "south out of the Ravaged Path");
        if (out.stop != WalkStop::TRANSITION && out.stop != WalkStop::LIMIT){
            fail_step(stream, "Ravaged Path: the walk south from a tile west of the corridor's end did not leave the cave.");
        }
        context.wait_for(2000ms);
    }
}

//  01.10: Route 204 north on the human's line, into Floaroma.
void route_204_to_floaroma(VideoStream& stream, ProControllerContext& context){
    // The line a human walked (Maps/Route204North.json,
    // the region floaroma_waypoints): a tile south out of the cave's mouth, west along
    // the row under the cliff past the Youngster who wanders there, up the
    // stairs, east along the corridor into Aroma Lady Taylor's battle, north
    // through the tall grass, west below the flower beds, and north between
    // the beds into Floaroma. Whole tiles on the D-pad, corner to corner.
    {
        RouteTrainers trainers{route204_north_trainer_sights(), [&](const std::string& who){
            const bool doubles = who.find("Twins") != std::string::npos || who.find("someone") != std::string::npos;
            talk_or_fight(stream, context, who, TalkExpect::TALK_OR_BATTLE, doubles ? BattleFormat::DOUBLES : BattleFormat::SINGLES);
        }};
        std::vector<Pokemon::TilePos> at{map_point("Route204North", "cave_exit")};
        at = walk_waypoints(stream, context, run_from_wild_battle, route204_north_maps(), at, trainers, WaypointWalkPlan{map_region("Route204North", "floaroma_waypoints"), "To the path into Floaroma", waypoint_plan_costs()});
        if (!std::all_of(at.begin(), at.end(), in_floaroma_path)){
            fail_step(stream, "Route 204: the walk did not end in the path into Floaroma.");
        }
        // North until the town's banner shows: the checkpoint ends right
        // there, where the save goes (r247a walked 20 s on and overshot).
        const WalkResult in = walk_until_dpad(stream, context, run_from_wild_battle, {0, 1}, 12000ms, "north into Floaroma", {.arrived = location_banner_showing});
        if (in.stop != WalkStop::ARRIVED){
            fail_step(stream, "Route 204: no location banner walking north into Floaroma.");
        }
        context.wait_for(500ms);
        const VideoSnapshot banner = stream.video().snapshot();
        const std::string read = banner ? read_location_banner(*banner.frame) : "";
        if (!location_banner_matches(read, "Floaroma Town")){
            fail_step(stream, "Route 204: the banner read \"" + read + "\", not Floaroma Town.");
        }
        stream.log("[01.10] In Floaroma Town; the banner read \"" + read + "\".", COLOR_GREEN);
    }
}

//  01.11: west 8 along the flower field's bottom row, north until the old woman stops us.
void west_8_north_to_the_old_woman(VideoStream& stream, ProControllerContext& context){
    {
        const WalkResult west = walk_tiles_dpad(stream, context, run_from_wild_battle, {-1, 0}, 8, "west 8 to the flower field");
        if (west.stop != WalkStop::LIMIT || west.moved_chunks < 7){
            fail_step(stream, "Floaroma: west 8 to the flower field moved " + std::to_string(west.moved_chunks) + " tiles.");
        }
        //  Blocked, or out of taps with the last ones bumping: either way
        //  she is in front of us (r250a: 3 steps, 2 bumps, "limit").
        const WalkResult north = walk_tiles_dpad(stream, context, run_from_wild_battle, {0, 1}, 5, "north until the old woman");
        if (north.stop != WalkStop::BLOCKED && north.moved_chunks >= 5){
            fail_step(stream, "Floaroma: nothing stopped the walk north to the old woman.");
        }
    }
}

//  01.11: one tile west, then a tap north: the old man stands there, so the tap only turns.
void below_the_old_man(VideoStream& stream, ProControllerContext& context){
    const WalkResult west = walk_tiles_dpad(stream, context, run_from_wild_battle, {-1, 0}, 1, "west 1 below the old man");
    if (west.moved_chunks < 1){
        fail_step(stream, "Floaroma: the step west below the old man did not move.");
    }
    const WalkResult north = walk_tiles_dpad(stream, context, run_from_wild_battle, {0, 1}, 3, "north until the old man");
    if (north.stop != WalkStop::BLOCKED && north.moved_chunks >= 3){
        fail_step(stream, "Floaroma: nothing stopped the walk north to the old man.");
    }
}

}


void add_route204_events(StoryEvents& events){
    events.emplace("north_out_of_jubilife", console_event(north_out_of_jubilife));
    events.emplace("route_204_south_to_the_door", console_event(route_204_south_to_the_door));
    events.emplace("into_the_ravaged_path", console_event(into_the_ravaged_path));
    events.emplace("through_the_ravaged_path", console_event(through_the_ravaged_path));
    events.emplace("route_204_to_floaroma", console_event(route_204_to_floaroma));
    events.emplace("west_8_north_to_the_old_woman", console_event(west_8_north_to_the_old_woman));
    events.emplace("below_the_old_man", console_event(below_the_old_man));
}



}
}
}
