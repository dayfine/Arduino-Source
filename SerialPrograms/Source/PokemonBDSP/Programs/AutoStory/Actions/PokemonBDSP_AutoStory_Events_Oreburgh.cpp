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

#include <algorithm>
#include <format>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryTrainers.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteTiming.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkResult.h"
#include "PokemonBDSP_AutoStory_BadgeCheck.h"
#include "PokemonBDSP_AutoStory_GymOpponent.h"
#include "PokemonBDSP_AutoStory_TalkFight.h"
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

//  The gym's column as tiles north of the entrance: where the farthest gym
//  trainer stops us, and the tile in front of Roark.
GymColumn oreburgh_gym_column(){
    const Pokemon::TilePos entrance = map_point("OreburghGym", "entrance");
    const Pokemon::TilePos roark = map_point("OreburghGym", "roark");
    int last_trainer = 0;
    for (const TrainerSight& trainer : oreburgh_gym_trainer_sights()){
        last_trainer = std::max(last_trainer, entrance.y - trainer.pos.y);
    }
    return GymColumn(last_trainer, entrance.y - roark.y - 1, ROUTE_MS_PER_TILE_UP_DOWN);
}

//  How a meeting with someone in the gym ended.
enum class GymMeeting{
    TALKED,         //  No battle: someone beaten, or a line only.
    LOST,           //  A whiteout; we are in the Pokemon Center.
    TRAINER_WON,
    LEADER_WON,
};

//  A talk has opened. Who it is, is what the battle's intro text says ("You
//  are challenged by Gym Leader Roark!"), read on the screen by
//  fight_story_battle(): never the count of trainers beaten or the distance
//  walked (lab-B, 2026-11-12 22:50: the second trainer, met by a blocked walk
//  and an A press, was fought as Roark, and 01.05 saved with no badge; 23:49:
//  after a reload Roark was "Gym trainer 1"). The column only hints, and says
//  how to wait: Roark's lines pause for more than 4 s before the battle
//  (r210d: "only talked"), so up at his tile a battle is expected. An intro
//  that was not read fails the step: the next one would be a guess.
GymMeeting meet_gym_opponent(VideoStream& stream, ProControllerContext& context, const GymColumn& column){
    const bool leader_hint = column.leader_here();
    stream.log(std::format("[01.04] About {:.1f} tiles up the gym (a hint, not the opponent: {}).",
        column.tiles(), leader_hint ? "Roark's tile" : "a trainer's"));
    //  His badge speech is over when talk_or_fight_who() returns (confirm_battle_overworld()).
    const TalkFight met = talk_or_fight_who(stream, context, "Gym opponent", leader_hint ? TalkExpect::BATTLE : TalkExpect::TALK_OR_BATTLE);
    if (!met.fought){
        return GymMeeting::TALKED;
    }
    if (in_pokemon_center(stream)){
        return GymMeeting::LOST;
    }
    stream.log(std::format("[01.04] The intro text says the opponent was the {}.", battle_opponent_name(met.opponent)));
    switch (met.opponent){
    case BattleOpponent::LEADER:
        return GymMeeting::LEADER_WON;
    case BattleOpponent::TRAINER:
        return GymMeeting::TRAINER_WON;
    case BattleOpponent::UNREAD:
        break;
    }
    fail_step(stream, "Oreburgh Gym: the battle's intro text was not read (English text only), so the opponent is not known to be a trainer or Roark.");
}

//  Inside the Oreburgh Gym: straight up the middle, fighting the trainers
//  who stop us, to Roark on the top tier. Returns whether Roark was beaten;
//  false after a whiteout, which leaves us in the Pokemon Center.
//  Walking straight up meets both gym trainers (one at the entrance, one on
//  the second tier); their experience helps an under-levelled team, and
//  after a whiteout they stay beaten. Butters'
//  route goes round them.
bool oreburgh_gym_battles(VideoStream& stream, ProControllerContext& context, GymColumn& column){
    column.restart();
    // The entry camera pans for about 7.5 s (Butters' reference video).
    context.wait_for(8000ms);
    if (dialogue_opens(stream, context, 3000ms)){
        if (meet_gym_opponent(stream, context, column) == GymMeeting::LOST){
            return false;
        }
    }
    size_t stuck = 0;
    for (size_t leg = 0; leg < 10; leg++){
        const WalkResult up = walk_until_no_wild(stream, context, {0, 1}, 8000ms, "north up the gym");
        column.walked_north(up.walked);
        if (up.stop == WalkStop::DIALOG){
            switch (meet_gym_opponent(stream, context, column)){
            case GymMeeting::LOST:
                return false;
            case GymMeeting::LEADER_WON:
                return true;
            default:
                break;
            }
            continue;
        }
        if (up.stop != WalkStop::BLOCKED){
            continue;
        }
        // Roark stands at the top facing us; a wall or a stair edge says
        // nothing to A. Whoever answers is the next opponent in line, which
        // is a gym trainer until both are beaten. The walk left us facing
        // north: a turn tap that way would be a step.
        pbf_press_button(context, BUTTON_A, 80ms, 200ms);
        if (dialogue_opens(stream, context, 3000ms)){
            switch (meet_gym_opponent(stream, context, column)){
            case GymMeeting::LOST:
                return false;
            case GymMeeting::LEADER_WON:
                return true;
            default:
                break;
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
void gym_until_roark_beaten(VideoStream& stream, ProControllerContext& context, Pokemon::StoryCheckLevel checks){
    // Losing costs some money and keeps the experience, so each try comes
    // back stronger: Roark won twice by hand against Piplup Lv 10 and 14 and
    // lost the third time.
    GymColumn column = oreburgh_gym_column();
    bool won = false;
    for (size_t attempt = 0; attempt < 5 && !won; attempt++){
        oreburgh_street_to_gym(stream, context);
        won = oreburgh_gym_battles(stream, context, column);
        if (!won){
            stream.log("[01.04] Lost in the gym (try " + std::to_string(attempt + 1) + "); back from the Center.", COLOR_ORANGE);
            walk_out_of_center(stream, context, "Oreburgh");
        }
    }
    if (!won){
        fail_step(stream, "Oreburgh Gym: lost to Roark five times.");
    }
    //  The Coal Badge is the gym's first (menu with 5 icons: no Map yet).
    verify_badge(stream, context, checks, 0, "Coal Badge", 5);
}

} // namespace


void add_oreburgh_events(StoryEvents& events){
    events.emplace("gym_until_roark_beaten", [](StoryExecutor& executor, Pokemon::RunState&, const Pokemon::StoryStep&){
        gym_until_roark_beaten(executor.stream(), executor.context(), executor.env().checks);
        return Pokemon::outcome_ok();
    });
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
