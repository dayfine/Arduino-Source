/*  BDSP AutoStory - Wild battle escape policy tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WildBattlePolicy_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WildBattlePolicy_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  The escape policy's decisions (PokemonBDSP_AutoStory_WildBattlePolicy.h)
//  on scripted Run outcomes: when it escapes, when it fights a battle the
//  game will not let it leave, and when it gives up.
void add_tests_AutoStory_WildBattlePolicy(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
