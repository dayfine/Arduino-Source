/*  PokemonLZA AutoStory - Scene machine replay Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySceneReplay_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStorySceneReplay_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the scene machine's replays of console runs: the looks its log
//  recorded, played back on a scripted backend, must reach the state the
//  plan's step ends on (passing runs, and runs a guard stopped or a blind
//  press looped). No frames needed.
void add_tests_AutoStorySceneReplay(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
