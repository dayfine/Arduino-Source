/*  AutoStory Evolution Wait Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryEvolutionWait_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryEvolutionWait_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The battle loop's evolution holds and its stop for a fainted lead after
//  the last turn, on scripted readings; and its read of that party list.
void add_tests_AutoStoryEvolutionWait(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
