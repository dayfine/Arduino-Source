/*  AutoStory Pacing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A capture's or grind's search for a wild battle: off the Bicycle, then
 *  pacing in the patch from where the last battle left the player.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPacing_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPacing_H

#include "Common/Cpp/Logging/AbstractLogger.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldBike.h"
#include "PokemonFRLG_AutoStoryCapture.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  Before pacing: gets the player off the Bicycle with SELECT, checked,
//  and once more if they still ride. On the Bicycle a tap facing another
//  way steps as well as turns, so pacing's turns walk it off the patch
//  (console B, 2026-11-09, 00.30 run 5: Route 7's grass). Returns whether
//  the player is on foot.
bool select_off_bike(const OverworldWalkSteps::BikeSelectCalls& calls);

//  One search for a wild battle, as a capture or a grind paces: off the
//  Bicycle (select_off_bike()), then pace_until_battle(). `steps` keeps
//  the tile pacing left the player on from one search to the next, so the
//  pace never starts a tile out. False if no battle came, or the player
//  rides on.
bool find_paced_encounter(Logger& logger, const OverworldWalkSteps::BikeSelectCalls& bike, PatchSteps& steps);

//  select_off_bike()'s calls on the console: SELECT is the Switch's MINUS,
//  and the player rides while no standing sprite shows (OverworldWalkSteps::screen_shows_riding()).
OverworldWalkSteps::BikeSelectCalls console_bike_select(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
