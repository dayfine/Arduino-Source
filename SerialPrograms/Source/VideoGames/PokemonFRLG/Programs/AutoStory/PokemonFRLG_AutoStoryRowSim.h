/*  AutoStory Row Sim
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story row's steps on the OverworldSim, for the simulation tests of
 *  each stage's rows: the walks as the program plans or lays them out,
 *  the steps into trainers' lines and their battles.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRowSim_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRowSim_H

#include <optional>
#include <string>
#include <vector>
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The route the program walks for `step`, a ROUTE or LEGS, from `at`:
//  planned, or the legs laid out.
std::optional<std::vector<RouteStep>> walked_route(const StoryPose& at, const StoryStep& step);

//  Runs `step` of row `id` on `sim` as the program does: routes and legs
//  by the sim's walker from `at`, where the table says the player stands,
//  fighting a trainer who sees the player on the way; a hop down a ledge
//  (hop()) or a step onto the tile before one (step_onto()), to where the
//  table says; a step one tap at a
//  time, or for a step into a trainer's line by step_until_seen(), each
//  watched for the trainer; a battle from the trainer's message, which
//  must be up; a cut tree cut and stepped onto. Moves `at` to where the
//  table says the step ends. Returns the failure, or empty.
std::string row_step_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
