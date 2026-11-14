/*  Pokemon FRLG Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <fstream>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/StaticGlobals.h"
#include "Inference/Box/PokemonFRLG_BoxCellOccupiedDetector.h"
#include "Inference/Box/PokemonFRLG_BoxCellReader.h"
#include "Inference/Box/PokemonFRLG_BoxGenderReader.h"
#include "Inference/Box/PokemonFRLG_BoxSelectionModeDetector.h"
#include "Inference/Box/PokemonFRLG_PcScreenReader.h"
#include "Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "Inference/Dialogs/PokemonFRLG_PrizeSelectDetector.h"
#include "Inference/Map/PokemonFRLG_PokemonLeagueDetectors.h"
#include "Inference/Menus/PokemonFRLG_BagDetector.h"
#include "Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "Inference/Menus/PokemonFRLG_OptionMenuReader.h"
#include "Inference/Menus/PokemonFRLG_PartyEmptySlotDetector.h"
#include "Inference/Menus/PokemonFRLG_PartyHeldItemDetector.h"
#include "Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "Inference/Menus/PokemonFRLG_SaveCardReader.h"
#include "Inference/Menus/PokemonFRLG_SummaryMovesReader.h"
#include "Inference/Menus/PokemonFRLG_TeachScreenReader.h"
#include "Inference/PokemonFRLG_BattleLevelUpReader.h"
#include "Inference/PokemonFRLG_BattleSelectionArrowDetector.h"
#include "Inference/PokemonFRLG_BattleTextBoxDetector.h"
#include "Inference/PokemonFRLG_PartyLevelUpReader.h"
#include "Inference/PokemonFRLG_ShinySymbolDetector.h"
#include "Inference/PokemonFRLG_StatsReader.h"
#include "Inference/PokemonFRLG_TrainerIdReader.h"
#include "Inference/PokemonFRLG_WildEncounterReader.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryScreenEvents_Tests.h"
#include "Programs/BoxOrganization/PokemonFRLG_BoxSorter_Tests.h"
#include "Overworld/PokemonFRLG_OverworldMaps.h"
#include "Overworld/PokemonFRLG_OverworldReanchor.h"
#include "Overworld/PokemonFRLG_OverworldRecovery.h"
#include "Overworld/PokemonFRLG_OverworldRoute.h"
#include "Overworld/PokemonFRLG_OverworldSim.h"
#include "Overworld/PokemonFRLG_OverworldWalk.h"
#include "Overworld/PokemonFRLG_PlayerFacingReader.h"
#include "Programs/PokemonFRLG_NamingScreenKeyboard_Tests.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryBattle.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryCapture.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryPaceSim.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryPcStorage.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryAbilities.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryRngLoop.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryRows_Tests.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryTable.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryTrashCans.h"
#include "Programs/AutoStory/Data/Json_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_GiftRngAttempts_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngPlanInputs_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngPlanLog_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngPlanTracker_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngPlan_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngSteps_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngTargetSearch_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_StarterRngLoop_Tests.h"
#include "Tests/TestUtils.h"

#include "PokemonFRLG_Tests.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The menu readers AutoStory brought: the bag's and Marts' lists, the
//  options, the save card, the summary's moves and the teach screens, and
//  the screen events.
static void add_tests_AutoStoryReaders(UnitTestDatabase& database){
    add_tests_ItemListReader(database);
    add_tests_OptionMenuReader(database);
    add_tests_SaveCardReader(database);
    add_tests_SummaryMovesReader(database);
    add_tests_TeachScreenReader(database);
    add_tests_AutoStoryScreenEvents(database);
}

//  The RNG programs' searches, plans and loops.
static void add_tests_RngPlanning(UnitTestDatabase& database){
    add_tests_RngTargetSearch(database);
    add_tests_RngPlanInputs(database);
    add_tests_RngPlan(database);
    add_tests_RngPlanTracker(database);
    add_tests_RngPlanLog(database);
    add_tests_RngSteps(database);
    add_tests_StarterRngLoop(database);
    add_tests_GiftRngAttempts(database);
}

//  The story's rows on the overworld sim, a file of tests per stage.
static void add_tests_AutoStoryRows(UnitTestDatabase& database){
    add_tests_AutoStoryVermilion(database);
    add_tests_AutoStoryVermilionGym(database);
    add_tests_AutoStoryCeladon(database);
    add_tests_AutoStorySnorlax(database);
    add_tests_AutoStorySlope(database);
}

void add_tests(UnitTestDatabase& database){
    add_tests_AdvanceWhiteDialogDetector(database);
    add_tests_AdvanceBattleDialogDetector(database);
    add_tests_BagDetector(database);
    add_tests_BattleLevelUpReader(database);
    add_tests_BattleMenuDetector(database);
    add_tests_BattleMoveSelectionArrow(database);
    add_tests_BoxCellOccupiedDetector(database);
    add_tests_BoxCellReader(database);
    add_tests_BoxGenderReader(database);
    add_tests_BoxSelectionModeDetector(database);
    add_tests_BoxSorter(database);
    add_tests_EliteFourRoomDetector(database);
    add_tests_HallOfFameSavingDetector(database);
    add_tests_PcScreenReader(database);
    add_tests_SelectionDialogDetector(database);
    add_tests_ShinySymbolDetector(database);
    add_tests_StatsReader(database);
    add_tests_WildEncounterReader(database);
    add_tests_TrainerIdReader(database);
    add_tests_PartyEmptySlotDetector(database);
    add_tests_PartyHeldItemDetector(database);
    add_tests_PartyLevelUpReader(database);
    add_tests_PartyMenuDetector(database);
    add_tests_PrizeSelectDetector(database);
    add_tests_PartySlotDetector(database);
    add_tests_BattleTextBoxDetector(database);
    add_tests_AutoStoryReaders(database);
    add_tests_OverworldMaps(database);
    add_tests_OverworldRoute(database);
    add_tests_OverworldWalk(database);
    add_tests_OverworldReanchor(database);
    add_tests_OverworldRecovery(database);
    add_tests_OverworldSim(database);
    add_tests_PlayerFacingReader(database);
    add_tests_AutoStory(database);
    add_tests_AutoStoryJson(database);
    add_tests_AutoStorySimulation(database);
    add_tests_AutoStoryRows(database);
    add_tests_AutoStoryTrashCans(database);
    add_tests_AutoStoryPaceSim(database);
    add_tests_AutoStoryItems(database);
    add_tests_AutoStoryRngLoop(database);
    add_tests_AutoStoryCapture(database);
    add_tests_AutoStoryPcStorage(database);
    add_tests_AutoStoryAbilities(database);
    add_tests_NamingScreen(database);
    add_tests_RngPlanning(database);
}


UnitTestResult check_against_golden_file(
    const std::string& image_path,
    const std::vector<std::string>& labels,
    const std::vector<std::string>& values
){
    if (labels.size() != values.size()){
        return "Error: check_against_golden_file() called with " +
               std::to_string(labels.size()) + " labels for " +
               std::to_string(values.size()) + " values.";
    }
    for (size_t c = 0; c < values.size(); c++){
        if (values[c].empty() || values[c].find_first_of(" \t\r\n") != std::string::npos){
            return "Error: value for \"" + labels[c] + "\" is empty or contains whitespace. "
                   "The golden file cannot represent it.";
        }
    }

    Filesystem::Path file_path(image_path);
    Filesystem::Path golden_path =
            file_path.parent_path() / ("_" + file_path.stem().string() + ".txt");

    if (STATIC_GLOBALS.GENERATE_TEST_GOLDEN_FILES){
        std::ofstream output_file(golden_path.stdpath());
        if (!output_file.is_open()){
            return "Error: cannot open " + golden_path.string() + " for writing.";
        }
        for (const std::string& value : values){
            output_file << value << std::endl;
        }
        return true;
    }

    std::vector<std::string> targets;
    if (!load_slug_list(golden_path.string(), targets)){
        return "Error: cannot load golden file " + golden_path.string() + ".";
    }
    if (targets.size() != values.size()){
        return "Error: golden file " + golden_path.string() + " has " +
               std::to_string(targets.size()) + " values, expected " +
               std::to_string(values.size()) + ".";
    }
    for (size_t c = 0; c < values.size(); c++){
        TEST_RESULT_COMPONENT_EQUAL_STR(values[c], targets[c], labels[c]);
    }
    return true;
}


}
}
}