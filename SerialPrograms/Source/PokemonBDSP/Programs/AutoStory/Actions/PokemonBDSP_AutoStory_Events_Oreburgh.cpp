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
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkResult.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

//  01.04 starts below Oreburgh's Center door (47, 21): 01.03's heal and
//  the walk out after a loss both leave us there. The Gym's door is on the
//  same street, so the D-pad taps west are the columns between the two
//  doors on the map (21). A hand count of 20, from before the maps, ended
//  a tile east of the Gym's door (lab-B, 2026-11-08 14:51).
size_t gym_taps_west_of_center(){
    const Pokemon::TilePos center = map_point("OreburghCity", "pokemon_center_door");
    const Pokemon::TilePos gym = map_point("OreburghCity", "gym_door");
    return static_cast<size_t>(center.x - gym.x);
}

void oreburgh_street_to_gym(VideoStream& stream, ProControllerContext& context){
    // From the Center's doorstep, straight west along its row to below the
    // gym's door, then north through it, all on the D-pad (whole tiles; see
    // gym_taps_west_of_center()). 01.04 always starts on the doorstep: its
    // own save, or the walk out of the Center after a loss. A brightness
    // check for "already in the gym" (gym 75 against street 104 by day, r74)
    // read the street at night (88, game clock 00:25) as the gym and talked
    // to the nurse as Roark (lab-B, 2026-11-08 14:25).
    context.wait_for_all_requests();
    dpad_tiles(stream, context, DPAD_LEFT, gym_taps_west_of_center(), "west to the gym's door");
    if (dpad_through_door_north(stream, context, "Oreburgh Gym door")){
        return;
    }
    //  The gym's wall stopped us: try the column west, then the one east.
    dpad_tiles(stream, context, DPAD_LEFT, 1, "a column west of the gym's door");
    if (dpad_through_door_north(stream, context, "Oreburgh Gym door")){
        return;
    }
    dpad_tiles(stream, context, DPAD_RIGHT, 2, "a column east of the gym's door");
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
            //  His badge speech is over when talk_or_fight() returns: the
            //  battle ends with its speeches (confirm_battle_overworld()).
            if (talk_or_fight(stream, context, "Roark", TalkExpect::BATTLE)){
                return !in_pokemon_center(stream);
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
