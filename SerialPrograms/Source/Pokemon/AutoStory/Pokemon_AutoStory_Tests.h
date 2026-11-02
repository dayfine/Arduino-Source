/*  AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The unit tests of the shared story engine (Pokemon/AutoStory).
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStory_Tests_H
#define PokemonAutomation_Pokemon_AutoStory_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


void add_tests_StoryEvents(UnitTestDatabase& database);
void add_tests_StorySegments(UnitTestDatabase& database);



}
}
#endif
