/*  PokemonLZA AutoStory - Scene step golden Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySceneGolden_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStorySceneGolden_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the scene steps' golden sequences: console runs' screens
//  played back through the built-in plan's step, each press checked
//  against the key its screen calls for. No frames needed.
void add_tests_AutoStorySceneGolden(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
