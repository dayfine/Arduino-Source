/*  Navigation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The unit tests of the shared grid navigation pieces.
 *  add_tests_Navigation() is the one entry point the unit test runner calls;
 *  a new shared Navigation suite registers there.
 *
 */

#ifndef PokemonAutomation_Pokemon_Navigation_Tests_H
#define PokemonAutomation_Pokemon_Navigation_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Grid/Maps/Pokemon_ConnectedMaps_Tests.h"
#include "Grid/Maps/Pokemon_MapData_Tests.h"
#include "Grid/Maps/Pokemon_MoveMechanics_Tests.h"
#include "Grid/Maps/Pokemon_RuntimeMap_Tests.h"
#include "Grid/Planning/Pokemon_Gates_Tests.h"
#include "Grid/Planning/Pokemon_LegPlanner_Tests.h"
#include "Grid/Planning/Pokemon_RobustLegPlanner_Tests.h"
#include "Grid/Planning/Pokemon_RouteScorer_Tests.h"
#include "Grid/Planning/Pokemon_WallRecovery_Tests.h"
#include "Grid/Planning/Pokemon_WanderRecovery_Tests.h"
#include "Grid/Trace/Pokemon_LegTrace_Tests.h"
#include "Grid/Trace/Pokemon_RouteLog_Tests.h"
#include "Grid/Walker/Pokemon_LegWalker_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Registers every shared Navigation suite, for the unit test runner.
inline void add_tests_Navigation(UnitTestDatabase& database){
    add_tests_ConnectedMaps(database);
    add_tests_MapData(database);
    add_tests_MoveMechanics(database);
    add_tests_RuntimeMap(database);
    add_tests_Gates(database);
    add_tests_LegPlanner(database);
    add_tests_RobustLegPlanner(database);
    add_tests_RouteScorer(database);
    add_tests_WallRecovery(database);
    add_tests_WanderRecovery(database);
    add_tests_RouteLog(database);
    add_tests_LegTrace(database);
    add_tests_LegWalker(database);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
