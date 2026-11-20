/*  PokemonLZA AutoStory - Checkpoint starts Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStarts_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryStarts_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the checkpoint start tests: where a run plays from by the
//  screen after the load, against the built-in plans and a made-up table;
//  and the console's frames from the test images
//  (PokemonLZA/AutoStoryStarts/), skipped when a frame is not there.
void add_tests_AutoStoryStarts(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
