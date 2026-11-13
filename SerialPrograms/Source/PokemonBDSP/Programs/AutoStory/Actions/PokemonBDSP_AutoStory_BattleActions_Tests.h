/*  BDSP AutoStory - Battle Actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_BattleActions_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_BattleActions_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Tests that need no console: the move picked from the menu as read, the
//  Potions, who a gym battle was against and the Trainer Card's badge check.
void add_tests_AutoStory_BattleActions(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
