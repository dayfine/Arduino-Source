/*  AutoStory Gates Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registration of the tests of the story's sims with gates: a Silph Co.
 *  card-key door and a Pokemon Mansion statue switch on the row sim.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryGates_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryGates_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  StoryMapsSim and OverworldStepSim honoring gates (PokemonFRLG_AutoStoryGates_Tests.cpp).
void add_tests_AutoStoryGates(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
