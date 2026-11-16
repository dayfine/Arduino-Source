/*  PokemonLZA AutoStory - Backup saves Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackups_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackups_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the rewind to the backup save's tests: the built-in backup
//  cards' checkpoints, and scripted runs that fail with and without an
//  autosave in the failed checkpoint (no console).
void add_tests_AutoStoryBackups(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
