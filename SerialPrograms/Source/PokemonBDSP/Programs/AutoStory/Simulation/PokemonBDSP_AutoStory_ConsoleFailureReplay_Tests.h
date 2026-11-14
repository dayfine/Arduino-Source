/*  BDSP AutoStory - Console failure replay tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_ConsoleFailureReplay_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_ConsoleFailureReplay_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  One test per fault file in ConsoleFailures/: the checkpoint it names runs
//  on the story simulation with only the file's faults injected, and must pass.
void add_tests_AutoStory_ConsoleFailureReplay(UnitTestDatabase& database);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
