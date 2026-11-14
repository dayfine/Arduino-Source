/*  AutoStory Fault Sweep Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFaultSweep_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFaultSweep_Tests_H

#include <optional>
#include <string>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The hide flag the script of `step`, a battle a talk starts, sets when
//  the trainer leaves after it: the sim, which plays no such battle, must
//  hide them for the row to go on.
std::optional<std::string> talk_battle_hide_flag(const Pokemon::StoryStep& step);

//  Each class of console failure injected at every row of the story on the
//  overworld sim; the rows the program still fails are known failing
//  (PokemonFRLG_AutoStoryFaultSweep_Tests.cpp).
void add_tests_AutoStoryFaultSweep(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
