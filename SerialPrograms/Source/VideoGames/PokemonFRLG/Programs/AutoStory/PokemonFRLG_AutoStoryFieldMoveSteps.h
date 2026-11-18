/*  AutoStory Field Move Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The field move GAME steps (PokemonFRLG_AutoStoryFieldMoves.h) as a
 *  checkpoint's JSON gives them, for tests.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFieldMoveSteps_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFieldMoveSteps_H

#include <cstddef>
#include <string>
#include <string_view>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A fly step to `town` with the Fly user in `slot` (from 1), ending on its
//  landing tile facing south.
Pokemon::StoryStep fly_step(std::string name, size_t slot, std::string_view town);
//  A surf step onto `water`'s tile, `water.facing` the way to it.
Pokemon::StoryStep surf_step(std::string name, Pokemon::GridPose water);


//  Registers the field moves' tests.
void add_tests_AutoStoryFieldMoves(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
