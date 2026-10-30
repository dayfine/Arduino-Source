/*  Navigation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_Navigation_Tests_H
#define PokemonAutomation_Pokemon_Navigation_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


void add_tests_LegPlanner(UnitTestDatabase& database);
void add_tests_LegWalker(UnitTestDatabase& database);
void add_tests_StoryEvents(UnitTestDatabase& database);



}
}
#endif
