/*  BDSP AutoStory Story Monte Carlo Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryMonteCarlo_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryMonteCarlo_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  Each row of the story table run on the story simulation for the pinned
//  seeds, disturbed (wild battles, drift, wanderers, a missed detection):
//  its pass rate and failing steps. PA_BDSP_STORY_MC_SEEDS sets the seed
//  count (20 by default), PA_BDSP_STORY_MC_ROWS the rows ("01.03,01.04").
void add_tests_AutoStory_StoryMonteCarlo(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
