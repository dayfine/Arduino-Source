/*  PokemonLZA AutoStory - Backup card reader Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackupCardReader_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackupCardReader_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the backup card reader's tests; console frames are skipped when missing.
void add_tests_AutoStoryBackupCardReader(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
