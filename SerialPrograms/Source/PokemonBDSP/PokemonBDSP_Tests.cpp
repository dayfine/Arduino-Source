/*  Pokemon BDSP Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Inference/PokemonBDSP_DialogDetector.h"
#include "Inference/PokemonBDSP_MenuDetector.h"
#include "Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Actions_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStory_FrozenTrainer_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStory_GridSim_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStory_FaultSweep_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStory_SimFailureCases_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_RecoveryWalk_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_Maps_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_PlannedRoute_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_SimulatedRoute_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_RoutePlanner_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_Segment00_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_StoryMaps_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_StoryWorld_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStory_StoryActions_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStory_StorySim_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStory_StorySimulation_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_StoryData_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_RouteBench_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_Walk_Tests.h"
#include "Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WildBattlePolicy_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStory_WallBattleSim_Tests.h"
#include "Programs/AutoStory/Simulation/PokemonBDSP_AutoStory_ConsoleFailureReplay_Tests.h"
#include "PokemonBDSP_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



void add_tests(UnitTestDatabase& database){
    add_tests_DialogDetector(database);
    add_tests_MenuDetector(database);
    add_tests_AutoStory_Actions(database);
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
    add_tests_AutoStory_PlannedRoute(database);
    add_tests_AutoStory_SimulatedRoute(database);
    add_tests_AutoStory_GridSim(database);
    add_tests_AutoStory_FrozenTrainer(database);
    add_tests_AutoStory_WallBattleSim(database);
    add_tests_AutoStory_RecoveryWalk(database);
    add_tests_AutoStory_StorySim(database);
    add_tests_AutoStory_SimFailureCases(database);
    add_tests_AutoStory_ConsoleFailureReplay(database);
    add_tests_AutoStory_FaultSweep(database);
    add_tests_AutoStory_StoryActions(database);
}



}
}
}
