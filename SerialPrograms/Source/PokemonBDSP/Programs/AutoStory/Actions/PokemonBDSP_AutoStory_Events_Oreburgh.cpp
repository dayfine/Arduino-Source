/*  BDSP AutoStory - Events: Oreburgh Gym (01.04)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.04's walk into the Oreburgh Gym and up it to Roark, again from the
 *  Pokemon Center after each loss, as it ran on the console (2026-10-29 to
 *  10-31). A loss keeps the experience, which a reload of the row's save
 *  would not.
 *
 */

#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

//  Oreburgh, from the street in front of the Pokemon Center (its door or its
//  front wall) into the gym, as walked by hand on 2026-10-29: west to the
//  cliff, south to the posts, a row up, then 17 tiles east is the gym door's
//  column. The hold is checked by the door itself: walk north, and if the
//  gym's wall stops us, try the columns either side.
//  D-pad moves from the Center's doorstep (as walk_out_of_center() leaves us
//  there) to below the gym's door: 20. The program's own runs landed 2 tiles
//  west with 22 and 1 tile west with 21 (r217d, r218d, the same tiles each
//  time). By hand, after a stick exit, it took 22 taps; the hand exit and
//  count did not carry over to the program's. Stick holds drift: 160 ms moved
//  36 to 48 px, so 18 or 19 of them missed the door by a part of a tile.
constexpr size_t GYM_DPAD_TAPS_WEST_OF_CENTER = 20;

void oreburgh_street_to_gym(VideoStream& stream, ProControllerContext& context){
    // From the Center's doorstep, straight west along its row to below the
    // gym's door, then north through it, all on the D-pad (whole tiles; see
    // GYM_DPAD_TAPS_WEST_OF_CENTER). 01.04 always starts on the doorstep: its
    // own save, or the walk out of the Center after a loss. A brightness
    // check for "already in the gym" (gym 75 against street 104 by day, r74)
    // read the street at night (88, game clock 00:25) as the gym and talked
    // to the nurse as Roark (lab-B, 2026-11-08 14:25).
    context.wait_for_all_requests();
    dpad_tiles(stream, context, DPAD_LEFT, GYM_DPAD_TAPS_WEST_OF_CENTER, "west to the gym's door");
    if (dpad_through_door_north(stream, context, "Oreburgh Gym door")){
        return;
    }
    fail_step(stream, "Oreburgh: no column near the gym's door went in.");
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
        const WalkResult up = walk_until_no_wild(stream, context, {0, 1}, 8000ms, "north up the gym");
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
        // nothing to A. Only Roark starts a battle from our A here. The walk
        // left us facing north: a turn tap that way would be a step.
        pbf_press_button(context, BUTTON_A, 80ms, 200ms);
        if (dialogue_opens(stream, context, 3000ms)){
            // His lines pause for more than 4 s before the battle (r210d: "only
            // talked"), so the battle is expected.
            if (talk_or_fight(stream, context, "Roark", TalkExpect::BATTLE)){
                if (in_pokemon_center(stream)){
                    return false;
                }
                finish_dialogue(stream, context, BUTTON_B, "Roark after the battle");
                return true;
            }
            continue;
        }
        if (up.walked <= 800ms && ++stuck >= 3){
            fail_step(stream, "Oreburgh Gym: stuck on the way up, and nobody answers A.");
        }
    }
    fail_step(stream, "Oreburgh Gym: did not reach Roark.");
}

//  01.04: west along the row into the gym and up it to Roark, again from the Center after a loss.
void gym_until_roark_beaten(VideoStream& stream, ProControllerContext& context){
    // Losing costs some money and keeps the experience, so each try comes
    // back stronger: Roark won twice by hand against Piplup Lv 10 and 14 and
    // lost the third time.
    bool won = false;
    for (size_t attempt = 0; attempt < 5 && !won; attempt++){
        oreburgh_street_to_gym(stream, context);
        won = oreburgh_gym_battles(stream, context);
        if (!won){
            stream.log("[01.04] Lost in the gym (try " + std::to_string(attempt + 1) + "); back from the Center.", COLOR_ORANGE);
            walk_out_of_center(stream, context, "Oreburgh");
        }
    }
    if (!won){
        fail_step(stream, "Oreburgh Gym: lost to Roark five times.");
    }
}

} // namespace


void add_oreburgh_events(StoryEvents& events){
    events.emplace("gym_until_roark_beaten", console_event(gym_until_roark_beaten));
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
