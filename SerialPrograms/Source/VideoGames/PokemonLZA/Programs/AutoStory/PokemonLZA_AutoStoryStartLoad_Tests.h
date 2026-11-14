/*  PokemonLZA AutoStory - Start load Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStartLoad_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryStartLoad_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The loads a run makes before its first checkpoint and the shared
//  launch's request for each: no console.
void add_tests_AutoStoryStartLoad(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
