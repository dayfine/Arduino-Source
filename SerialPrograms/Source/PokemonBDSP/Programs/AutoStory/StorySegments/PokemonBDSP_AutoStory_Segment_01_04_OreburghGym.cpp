/*  BDSP AutoStory - Segment 01.04: Oreburgh Gym
 *  Into the gym, up past its trainers to Roark, again from the Pokemon
 *  Center after a loss, and out with the Coal Badge.
 *
 *  Start: outside the Oreburgh Mine, after talking to Roark.
 *  End:   outside the Oreburgh Gym's door, with the Coal Badge.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdlib>

#include "../Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"

#include "PokemonBDSP_AutoStory_Segment_01.h"
#include "PokemonBDSP_AutoStory_Segment_01_Shared.h"

using namespace std::chrono_literals;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;


namespace{

//  For a player inside a Pokemon Center after a whiteout, at the counter:
//  out through the door. The door is straight south of the nurse in most
//  Centers; in Oreburgh's it was two tiles east of where we stood (by hand).
void walk_out_of_center(VideoStream& stream, ProControllerContext& context, const std::string& label){
    finish_dialogue(stream, context, 0, label + ": the nurse");
    const int shifts[] = {0, 2, -4};
    for (int shift : shifts){
        if (shift != 0){
            hold_tiles(stream, context, shift > 0 ? Direction::EAST : Direction::WEST, (size_t)std::abs(shift));
        }
        if (walk_dir(stream, context, Direction::SOUTH, 8000ms, label + ": out of the Center").stop == WalkStop::TRANSITION){
            context.wait_for(2000ms);
            return;
        }
    }
    fail(stream, label + ": could not find the Pokemon Center's door.");
}

//  Oreburgh, from the street in front of the Pokemon Center (its door or its
//  front wall) into the gym, as walked by hand on 2026-10-29: west to the
//  cliff, south to the posts, a row up, then 17 tiles east is the gym door's
//  column. The hold is checked by the door itself: walk north, and if the
//  gym's wall stops us, try the columns either side.
void oreburgh_street_to_gym(VideoStream& stream, ProControllerContext& context){
    if (walk_dir(stream, context, Direction::WEST, 15000ms, "west to the cliff").stop != WalkStop::BLOCKED){
        fail(stream, "Oreburgh: walking west from the Center did not stop at the cliff.");
    }
    if (walk_dir(stream, context, Direction::SOUTH, 3000ms, "south to the posts").stop != WalkStop::BLOCKED){
        fail(stream, "Oreburgh: walking south did not stop at the posts.");
    }
    hold_tiles(stream, context, Direction::NORTH, 1);
    hold_tiles(stream, context, Direction::EAST, 17);
    // Cumulative steps: here, a tile east, a tile west, two east, two west.
    const int steps[] = {0, 1, -2, 3, -4};
    for (int step : steps){
        if (step != 0){
            hold_tiles(stream, context, step > 0 ? Direction::EAST : Direction::WEST, (size_t)std::abs(step));
        }
        const WalkResult door = walk_dir(stream, context, Direction::NORTH, 4000ms, "north into the gym");
        // The door's fade can read as the time limit rather than a black
        // screen (by hand: "limit after 4000ms", and we were inside).
        if (door.stop == WalkStop::TRANSITION || door.stop == WalkStop::LIMIT){
            context.wait_for(1000ms);
            return;
        }
        if (door.stop == WalkStop::DIALOG){
            talk_or_fight(stream, context, "Someone outside the gym");
        }
    }
    fail(stream, "Oreburgh: no column near the gym's door went in.");
}

//  Inside the Oreburgh Gym: straight up the middle, fighting the trainers
//  who stop us, to Roark on the top tier. Returns whether Roark was beaten;
//  false after a whiteout, which leaves us in the Pokemon Center.
//  Walking straight up meets both gym trainers (one at the entrance, one on
//  the second tier); their experience helps an under-levelled team, and
//  after a whiteout they stay beaten. Butters' route goes round them.
bool oreburgh_gym_battles(VideoStream& stream, ProControllerContext& context){
    // The entry camera pans for about 7.5 s (Butters' reference video).
    context.wait_for(8000ms);
    if (dialogue_opens(stream, context, 3000ms)){
        if (talk_or_fight(stream, context, "Gym trainer at the entrance") && in_pokemon_center(stream)){
            return false;
        }
    }
    size_t stuck = 0;
    for (size_t leg = 0; leg < 10; leg++){
        const WalkResult up = walk_dir(stream, context, Direction::NORTH, 8000ms, "north up the gym");
        if (up.stop == WalkStop::DIALOG){
            if (talk_or_fight(stream, context, "Gym trainer") && in_pokemon_center(stream)){
                return false;
            }
            continue;
        }
        if (up.stop != WalkStop::BLOCKED){
            continue;
        }
        // Roark stands at the top facing us; a wall or a stair edge says
        // nothing to A. Only Roark starts a battle from our A here.
        route_turn(stream, context, Direction::NORTH);
        pbf_press_button(context, BUTTON_A, 80ms, 200ms);
        if (dialogue_opens(stream, context, 3000ms)){
            if (talk_or_fight(stream, context, "Roark")){
                if (in_pokemon_center(stream)){
                    return false;
                }
                finish_dialogue(stream, context, 0, "Roark after the battle");
                return true;
            }
            continue;
        }
        if (up.walked <= 800ms && ++stuck >= 3){
            fail(stream, "Oreburgh Gym: stuck on the way up, and nobody answers A.");
        }
    }
    fail(stream, "Oreburgh Gym: did not reach Roark.");
}

void oreburgh_gym(VideoStream& stream, ProControllerContext& context){
    StepLog step(stream, "01.04", "From the Oreburgh Mine to the Coal Badge", {
        "North to the Pokemon Center's front",
        "West to the cliff, south to the posts, east along the street into the gym",
        "Up the gym, fighting its trainers, and beat Roark (after a loss: from the Center, again)",
        "Out of the gym",
    });
    step(1);
    // 01.03 leaves us below the mine's sign; a tile east is the column
    // straight up to the Center (by hand: blocked at its front after 6.4 s).
    hold_tiles(stream, context, Direction::EAST, 1);
    if (walk_dir(stream, context, Direction::NORTH, 12000ms, "north to the Center's front").stop != WalkStop::BLOCKED){
        fail(stream, "Oreburgh: walking north from the mine did not stop at the Pokemon Center.");
    }

    // Losing costs some money and keeps the experience, so each try comes
    // back stronger: Roark won twice by hand against Piplup Lv 10 and 14 and
    // lost the third time.
    bool won = false;
    for (size_t attempt = 0; attempt < 5 && !won; attempt++){
        step(2);
        oreburgh_street_to_gym(stream, context);
        step(3);
        won = oreburgh_gym_battles(stream, context);
        if (!won){
            stream.log("[01.04] Lost in the gym (try " + std::to_string(attempt + 1) + "); back from the Center.", COLOR_ORANGE);
            walk_out_of_center(stream, context, "Oreburgh");
        }
    }
    if (!won){
        fail(stream, "Oreburgh Gym: lost to Roark five times.");
    }

    step(4);
    // Back down the middle and out of the door.
    for (size_t leg = 0; leg < 4; leg++){
        const WalkResult down = walk_dir(stream, context, Direction::SOUTH, 10000ms, "south out of the gym");
        if (down.stop == WalkStop::TRANSITION){
            context.wait_for(2000ms);
            return;
        }
        if (down.stop == WalkStop::DIALOG){
            finish_dialogue(stream, context, 0, "Someone in the gym");
        }
    }
    fail(stream, "Oreburgh Gym: did not leave by its door.");
}

void checkpoint_0104(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
){
    checkpoint_reattempt_loop(env, context, options, stats,
        [&](size_t /*attempt*/){
            oreburgh_gym(env.console, context);
        },
        true
    );
}
}


std::string AutoStory_Checkpoint_0104::name()       const { return "01.04 - Oreburgh Gym"; }
std::string AutoStory_Checkpoint_0104::start_text() const { return AutoStory_Checkpoint_0103().end_text(); }
std::string AutoStory_Checkpoint_0104::end_text()   const { return "Outside the Oreburgh Gym's door, with the Coal Badge."; }

void AutoStory_Checkpoint_0104::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0104(env, context, options, stats);
}


}
}
}
