/*  FRLG Checkpoint Export Tests
 *  From: https://github.com/PokemonAutomation/
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryJson_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryJson_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  Register evaluated-table and non-default native-extension preservation tests.
void add_tests_AutoStoryJson(UnitTestDatabase& database);
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
