/*  Naming Screen Keyboard Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_NamingScreenKeyboard_Tests_H
#define PokemonAutomation_PokemonFRLG_NamingScreenKeyboard_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Registers the naming screen keyboard's tests: cursor moves, plans and
//  text checks, against pokefirered's src/naming_screen.c.
void add_tests_NamingScreen(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
