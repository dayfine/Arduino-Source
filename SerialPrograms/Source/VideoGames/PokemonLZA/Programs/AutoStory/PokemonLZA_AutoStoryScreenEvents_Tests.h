/*  PokemonLZA AutoStory - Screen events Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers PlzaScreenObserver's tests on console frames from
//  CommandLineTests/PokemonLZA/: its A prompt (INTERACT_PROMPT) and party
//  bar (PLAYER_CAN_MOVE) events.
void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
