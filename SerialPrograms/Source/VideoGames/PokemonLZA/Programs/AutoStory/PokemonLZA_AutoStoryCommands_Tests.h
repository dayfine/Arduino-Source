/*  PokemonLZA AutoStory - Command File Runner commands Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCommands_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryCommands_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Tests that need no console: each LZA command's arguments, and that the
//  Command File Runner finds every command with its kind once registered.
void add_tests_AutoStory_Commands(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
