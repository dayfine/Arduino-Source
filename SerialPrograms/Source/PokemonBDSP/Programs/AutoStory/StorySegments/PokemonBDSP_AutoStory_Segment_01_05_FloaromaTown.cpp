/*  BDSP AutoStory - Segment 01.05: Oreburgh City to Floaroma Town
 *  Back west through the Oreburgh Gate, Route 203 and Jubilife, north past
 *  Rowan's scene and the tag battle, through Route 204 and the Ravaged
 *  Path, to the Floaroma Pokemon Center.
 *
 *  Start: outside the Oreburgh Gym's door, with the Coal Badge.
 *  End:   in the Floaroma Town Pokemon Center, healed.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>

#include "../Data/PokemonBDSP_AutoStory_RavagedPath.h"
#include "../Data/PokemonBDSP_AutoStory_Route204.h"
#include "../Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "../Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "../Utils/PokemonBDSP_AutoStory_PokemonCenter.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"

#include "PokemonBDSP_AutoStory_Segment_01.h"
#include "PokemonBDSP_AutoStory_Segment_01_Shared.h"

using namespace std::chrono_literals;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;


namespace{

//  West until a transition, by the user's rule for a wall in the way: walk
//  north to the top wall, which pins the row, then try the rows south of it
//  one at a time. Trainers who stop us are fought. Returns at the first
//  transition (a building, a cave, a gate).
void west_until_transition(VideoStream& stream, ProControllerContext& context, const std::string& label, size_t max_legs){
    for (size_t leg = 0; leg < max_legs; leg++){
        const WalkResult west = walk_dir(stream, context, Direction::WEST, 20000ms, label);
        if (west.stop == WalkStop::TRANSITION){
            context.wait_for(2000ms);
            return;
        }
        if (west.stop == WalkStop::DIALOG){
            talk_or_fight(stream, context, label + ": someone in the way");
            continue;
        }
        if (west.stop != WalkStop::BLOCKED){
            continue;
        }
        walk_dir(stream, context, Direction::NORTH, 4000ms, label + ": north to the top wall");
        bool moved = false;
        for (size_t row = 0; row < 10 && !moved; row++){
            hold_tiles(stream, context, Direction::SOUTH, 1);
            const WalkResult probe = walk_dir(stream, context, Direction::WEST, 1500ms, label + ": west on the next row");
            moved = probe.stop != WalkStop::BLOCKED || probe.walked > 1200ms;
            if (probe.stop == WalkStop::TRANSITION){
                context.wait_for(2000ms);
                return;
            }
            if (probe.stop == WalkStop::DIALOG){
                talk_or_fight(stream, context, label + ": someone in the way");
            }
        }
        if (!moved){
            fail(stream, label + ": no row below the top wall leads west.");
        }
    }
    fail(stream, label + ": no transition after " + std::to_string(max_legs) + " legs.");
}

void oreburgh_to_floaroma(VideoStream& stream, ProControllerContext& context){
    StepLog step(stream, "01.05", "From the Oreburgh Gym to Floaroma Town", {
        "Up to the platform by Oreburgh Gate and into it (Barry bumps into us there)",
        "Through the Gate and west along Route 203 and Jubilife to the Route 218 gate",
        "Back east to Jubilife's north road and up it (Rowan, Dawn and two Galactic grunts: a tag battle)",
        "North on Route 204 into the Ravaged Path",
        "Through the Ravaged Path, smashing its rocks",
        "Route 204 north on its grid, past the twins, into Floaroma",
        "Heal at the Floaroma Pokemon Center",
    });

    step(1);
    // From the gym door, as walked by hand on 2026-10-29.
    hold_tiles(stream, context, Direction::SOUTH, 1);
    hold_tiles(stream, context, Direction::NORTH, 1);
    walk_dir(stream, context, Direction::WEST, 20000ms, "west to the stairs' cliff");
    walk_dir(stream, context, Direction::NORTH, 10000ms, "north along the cliff");
    hold_tiles(stream, context, Direction::EAST, 2);
    walk_dir(stream, context, Direction::NORTH, 5000ms, "north up the stairs");
    walk_dir(stream, context, Direction::WEST, 6000ms, "west along the platform");
    hold_tiles(stream, context, Direction::SOUTH, 1);
    // Barry runs into us on the platform ("*thud*") the first time; his lines
    // end with no battle.
    west_until_transition(stream, context, "Into Oreburgh Gate", 8);

    step(2);
    // The Gate's cave, Route 203 (trainers stop us) and Jubilife's road all
    // run west; the first door at the end is the Route 218 gate on
    // Jubilife's west edge (by hand: twice).
    west_until_transition(stream, context, "West to the Route 218 gate", 16);

    step(3);
    // Back out east into Jubilife: its north road is columns 43 to 46 of the
    // Jubilife grid, about 40 tiles east of the gate. Rowan's scene starts
    // on the way up it; a wall in a few tiles means we are off its column.
    if (walk_dir(stream, context, Direction::EAST, 6000ms, "back out of the gate").stop != WalkStop::TRANSITION){
        fail(stream, "Jubilife: did not leave the Route 218 gate eastwards.");
    }
    context.wait_for(2000ms);
    hold_tiles(stream, context, Direction::EAST, 40);
    const int steps[] = {0, 1, -2, 3, -4, 5, -6};
    bool scene = false;
    for (int s_step : steps){
        if (s_step != 0){
            hold_tiles(stream, context, s_step > 0 ? Direction::EAST : Direction::WEST, (size_t)std::abs(s_step));
        }
        const WalkResult up = walk_dir(stream, context, Direction::NORTH, 15000ms, "north up Jubilife's north road");
        if (up.stop == WalkStop::DIALOG){
            scene = true;
            break;
        }
        walk_dir(stream, context, Direction::SOUTH, 15000ms, "back down to the street");
    }
    if (!scene){
        fail(stream, "Jubilife: no column led up the north road to Rowan.");
    }
    if (!talk_or_fight(stream, context, "Galactic grunts with Dawn", true, true)){
        fail(stream, "Jubilife: Rowan's scene ended without the tag battle.");
    }
    if (in_pokemon_center(stream)){
        fail(stream, "Jubilife: lost the tag battle with Dawn.");
    }
    finish_dialogue(stream, context, 0, "Rowan and Dawn after the battle");

    step(4);
    // Route 204's south half: a trainer just past the sign, then the cave in
    // the cliff a column west of the path.
    for (size_t leg = 0; leg < 6; leg++){
        const WalkResult up = walk_dir(stream, context, Direction::NORTH, 15000ms, "north on Route 204");
        if (up.stop == WalkStop::TRANSITION){
            break;
        }
        if (up.stop == WalkStop::DIALOG){
            talk_or_fight(stream, context, "Route 204 trainer");
            continue;
        }
        if (up.stop == WalkStop::BLOCKED){
            hold_tiles(stream, context, Direction::WEST, 1);
        }
        if (leg == 5){
            fail(stream, "Route 204: did not reach the Ravaged Path.");
        }
    }
    context.wait_for(2000ms);

    step(5);
    // On the traced grid (Data/PokemonBDSP_AutoStory_RavagedPath): from the
    // south door up the stairs to the tile west of one of the two rocks
    // across the lower corridor, smash it, then east and south to the
    // corridor's east end; the way out is a tile west of it, south.
    {
        std::vector<Pokemon::TrainerSight> no_trainers;
        auto in_the_way = [&](const std::string& who){ talk_or_fight(stream, context, who); };
        std::vector<Pokemon::TilePos> at{RavagedPath::ENTRANCE};
        const Pokemon::MapSet rocks_in = ravaged_path_maps();
        at = walk_route(stream, context, rocks_in, at,
            [&](const Pokemon::TilePos& pos){
                const auto& before = RavagedPath::before_rocks();
                return std::find(before.begin(), before.end(), pos) != before.end();
            },
            no_trainers, in_the_way, "To a rock across the corridor");
        route_turn(stream, context, Direction::EAST);
        pbf_press_button(context, BUTTON_A, 80ms, 200ms);
        if (!dialogue_opens(stream, context, 2000ms)){
            fail(stream, "Ravaged Path: no Rock Smash offer facing the rock east of us.");
        }
        // "Would you like to use Rock Smash?": A for Yes, the animation, then
        // B for anything left (by hand, 2026-10-29).
        pbf_press_button(context, BUTTON_A, 80ms, 1500ms);
        pbf_press_button(context, BUTTON_A, 80ms, 4000ms);
        pbf_press_button(context, BUTTON_B, 80ms, 1500ms);
        context.wait_for_all_requests();
        // We may be on either row; the rock east of the other stays, but no
        // leg from our row crosses it.
        std::vector<Pokemon::TilePos> smashed;
        for (const Pokemon::TilePos& pos : RavagedPath::before_rocks()){
            smashed.push_back(pos.next(Direction::EAST));
        }
        const Pokemon::MapSet rocks_out = ravaged_path_maps(smashed);
        at = walk_route(stream, context, rocks_out, at,
            [&](const Pokemon::TilePos& pos){ return pos == RavagedPath::EAST_END; },
            no_trainers, in_the_way, "To the corridor's east end");
        hold_tiles(stream, context, Direction::WEST, 1);
        const WalkResult out = walk_dir(stream, context, Direction::SOUTH, 4000ms, "south out of the Ravaged Path");
        if (out.stop != WalkStop::TRANSITION && out.stop != WalkStop::LIMIT){
            fail(stream, "Ravaged Path: the walk south from a tile west of the corridor's end did not leave the cave.");
        }
        context.wait_for(2000ms);
    }

    step(6);
    // On the traced grid (Data/PokemonBDSP_AutoStory_Route204): from the
    // cave's door west and up the stairs, round the pocket the ledge closes,
    // east and north through the tall grass, and west below the flower beds,
    // where the twins see us; then north between the beds into Floaroma.
    {
        std::vector<Pokemon::TrainerSight> twins = Route204::north_trainer_sights();
        auto fight = [&](const std::string& who){
            const bool doubles = who.find("Twins") != std::string::npos || who.find("someone") != std::string::npos;
            talk_or_fight(stream, context, who, false, doubles);
        };
        std::vector<Pokemon::TilePos> at{Route204::CAVE_EXIT};
        at = walk_route(stream, context, route204_north_maps(), at, Route204::in_floaroma_path,
            twins, fight, "To the path into Floaroma");
        // Floaroma's arrival did not read as a black screen by hand ("limit
        // after 20000ms", and we stood in the town), so either will do.
        const WalkResult in = walk_dir(stream, context, Direction::NORTH, 20000ms, "north into Floaroma");
        if (in.stop == WalkStop::BLOCKED || in.stop == WalkStop::DIALOG){
            fail(stream, "Route 204: the walk north into Floaroma was stopped.");
        }
        context.wait_for(2000ms);
    }

    step(7);
    // We arrive in front of a house; the Center is a few tiles south-east.
    walk_dir(stream, context, Direction::SOUTH, 1500ms, "south off the house's path");
    hold_tiles(stream, context, Direction::EAST, 4);
    heal_at_pokemon_center(stream, context, "Floaroma Pokemon Center");
}

void checkpoint_0105(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
){
    checkpoint_reattempt_loop(env, context, options, stats,
        [&](size_t /*attempt*/){
            oreburgh_to_floaroma(env.console, context);
        },
        true
    );
}
}


std::string AutoStory_Checkpoint_0105::name()       const { return "01.05 - Oreburgh to Floaroma"; }
std::string AutoStory_Checkpoint_0105::start_text() const { return AutoStory_Checkpoint_0104().end_text(); }
std::string AutoStory_Checkpoint_0105::end_text()   const { return "In the Floaroma Town Pokemon Center, healed."; }

void AutoStory_Checkpoint_0105::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0105(env, context, options, stats);
}


}
}
}
