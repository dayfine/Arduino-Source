/*  PokemonLZA AutoStory - Engine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryEngine_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryEngine_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  PLZA's checkpoints and the new game's scene steps on the shared story engine,
//  with a scripted backend: no console.
void add_tests_AutoStoryEngine(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
