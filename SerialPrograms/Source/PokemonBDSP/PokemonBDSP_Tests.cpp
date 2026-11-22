/*  Pokemon BDSP Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Inference/PokemonBDSP_DialogDetector.h"
#include "Inference/PokemonBDSP_MenuDetector.h"
#include "Programs/AutoStory/Actions/PokemonBDSP_AutoStoryMenuActions_Tests.h"
#include "Programs/AutoStory/Actions/PokemonBDSP_AutoStorySettings_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStoryGridSim_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStoryFaultSweep_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStorySimFailureCases_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryRecoveryWalk_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryResources_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryLaunch_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryMaps_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryPlannedRoute_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStorySimulatedRoute_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryRoutePlanner_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStorySegment00_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryStoryMaps_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryWorld_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStoryActions_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStorySim_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStorySimulation_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryFiles_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryTable_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryRouteBench_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStoryWalk_Tests.h"
#include "Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWildBattlePolicy_Tests.h"
#include "Programs/AutoStory/Utils/PokemonBDSP_AutoStoryIndoors_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStoryWallBattleSim_Tests.h"
#include "Programs/RngManipulation/PokemonBDSP_StarterRng_Tests.h"
#include "PokemonBDSP_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



void add_tests(UnitTestDatabase& database){
    add_tests_DialogDetector(database);
    add_tests_MenuDetector(database);
    add_tests_AutoStory_Actions(database);
    add_tests_AutoStory_Settings(database);
    add_tests_AutoStory_Launch(database);
    add_tests_AutoStory_Maps(database);
    add_tests_AutoStory_StoryWorld(database);
    add_tests_AutoStory_RoutePlanner(database);
    add_tests_AutoStory_RouteBench(database);
    add_tests_AutoStory_StoryMaps(database);
    add_tests_AutoStory_Segment00(database);
    add_tests_AutoStory_StorySimulation(database);
    add_tests_AutoStory_StoryData(database);
    add_tests_AutoStory_StoryTable(database);
    add_tests_AutoStory_Walk(database);
    add_tests_AutoStory_WildBattlePolicy(database);
    add_tests_AutoStory_Indoors(database);
    add_tests_AutoStory_PlannedRoute(database);
    add_tests_AutoStory_SimulatedRoute(database);
    add_tests_AutoStory_GridSim(database);
    add_tests_AutoStory_WallBattleSim(database);
    add_tests_AutoStory_RecoveryWalk(database);
    add_tests_AutoStory_StorySim(database);
    add_tests_AutoStoryResources(database);
    add_tests_AutoStory_SimFailureCases(database);
    add_tests_AutoStory_FaultSweep(database);
    add_tests_AutoStory_StoryActions(database);
    add_tests_StarterRng(database);
}



}
}
}
