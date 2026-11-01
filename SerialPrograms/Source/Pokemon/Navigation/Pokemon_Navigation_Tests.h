/*  Navigation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_Navigation_Tests_H
#define PokemonAutomation_Pokemon_Navigation_Tests_H

#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


void add_tests_LegPlanner(UnitTestDatabase& database);
void add_tests_MapData(UnitTestDatabase& database);
void add_tests_StoryEvents(UnitTestDatabase& database);
void add_tests_StorySegments(UnitTestDatabase& database);
void add_tests_StorySteps(UnitTestDatabase& database);

//  (name, JSON text) of each map in Pokemon/Navigation/TestMaps/, built into
//  the program by pa_embed_text_files() (cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& navigation_test_map_json();



}
}
#endif
