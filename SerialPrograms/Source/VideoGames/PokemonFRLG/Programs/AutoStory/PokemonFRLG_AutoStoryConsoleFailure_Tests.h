/*  FRLG AutoStory - Console failure replay tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStory_ConsoleFailure_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStory_ConsoleFailure_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  One test per fault file in ConsoleFailures/ (Pokemon_LegReplay.h): the
//  walk it names runs on the overworld simulation with only the file's
//  faults injected, and must end where the route says. A file with a
//  "known_failing" fix is red on the walker that failed on the console: its
//  test passes while the walk is wrong and fails once it is right, so the
//  file's entry is then taken off.
void add_tests_AutoStoryConsoleFailure(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
