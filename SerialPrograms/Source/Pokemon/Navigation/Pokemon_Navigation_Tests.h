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


// Registers leg planner algorithm unit tests.
void add_tests_LegPlanner(UnitTestDatabase& database);
// Registers leg walker execution unit tests.
void add_tests_LegWalker(UnitTestDatabase& database);
//  Register shared wall-convergence policy tests.
void add_tests_WallRecovery(UnitTestDatabase& database);
// Registers map data parsing and lookup unit tests.
void add_tests_MapData(UnitTestDatabase& database);
// Registers route logging formatting unit tests.
void add_tests_RouteLog(UnitTestDatabase& database);
// Registers leg trace record and tracer unit tests.
void add_tests_LegTrace(UnitTestDatabase& database);
//  Registers route scorer unit tests.
void add_tests_RouteScorer(UnitTestDatabase& database);

//  (name, JSON text) of each map in Pokemon/Navigation/TestMaps/, built into
//  the program by pa_embed_text_files() (cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& navigation_test_map_json();



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
