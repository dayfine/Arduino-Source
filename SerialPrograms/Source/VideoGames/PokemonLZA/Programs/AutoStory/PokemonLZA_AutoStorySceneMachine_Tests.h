/*  PokemonLZA AutoStory - Scene machine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySceneMachine_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStorySceneMachine_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the scene machine's tests: its timeline on sequences of looks,
//  what each state does, and runs on a scripted backend. No frames needed.
void add_tests_AutoStorySceneMachine(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
