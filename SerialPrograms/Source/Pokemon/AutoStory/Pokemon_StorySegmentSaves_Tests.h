/*  Story Segment Saves Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySegmentSaves_Tests_H
#define PokemonAutomation_Pokemon_StorySegmentSaves_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Registers the tests of the checkpoint retry loop and of when the segment
//  runner and verify mode save.
void add_tests_StorySegmentSaves(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
