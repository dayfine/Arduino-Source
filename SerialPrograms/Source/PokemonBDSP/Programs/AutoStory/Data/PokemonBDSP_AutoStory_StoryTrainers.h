/*  BDSP AutoStory - Story trainers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The trainers on the story maps, with the sight ranges that fit the
 *  console: the maps do not say how far a trainer sees.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryTrainers_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryTrainers_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_TrainerSight.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Route 202's trainers, in world tiles of story_maps().
std::vector<TrainerSight> story_trainer_sights();

//  The trainers of a map, in its own tiles, with the sight ranges that fit
//  the console. A trainer who looks two ways is listed once per way.
std::vector<TrainerSight> route202_trainer_sights();
//  Returns trainer sights for Route 204 North.
std::vector<TrainerSight> route204_north_trainer_sights();
//  Returns trainer sights for Route 204 South.
std::vector<TrainerSight> route204_south_trainer_sights();
//  Returns trainer sights for Oreburgh Gym.
std::vector<TrainerSight> oreburgh_gym_trainer_sights();

//  Route 204 South's tiles in the trainers' sight that a walk can avoid:
//  all but Sarah's east line, which crosses the street (see the sights).
std::vector<Pokemon::TilePos> route204_south_tiles_to_avoid();


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
