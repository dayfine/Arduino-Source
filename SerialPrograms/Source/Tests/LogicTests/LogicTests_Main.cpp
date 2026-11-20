/*  Logic Tests Main
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  SerialProgramsLogicTests: runs the unit tests of code that needs no Qt, OpenCV,
 *  ONNX Runtime or Tesseract, and no test images. It is built only with
 *  -DPA_CORE_ONLY=ON -DPA_LOGIC_TESTS=ON (see cmake/LogicTests.cmake), so it can
 *  test refactors even when the full build's third-party downloads are broken.
 *
 *  This executable opens no serial port, camera or console: it only links the
 *  GUI-free CoreLib and the pure sources listed in LOGIC_LIBRARY_SOURCES. It is
 *  safe to run anywhere, unlike the SerialPrograms app.
 *
 *  Usage: SerialProgramsLogicTests [--filter <substring>]
 *  Runs every registered test (or those whose name contains <substring>) one at a
 *  time, prints one line per test and a summary, and returns 1 if any test failed.
 *
 *  A suite joins this runner when every file it includes is pure: add its sources to
 *  LOGIC_LIBRARY_SOURCES / LOGIC_TEST_SOURCES in cmake/SourceFiles.cmake and its
 *  add_tests_<Name>() call to make_logic_tests() below. The same suites also stay
 *  registered in the full test runner (ComputerPrograms_UnitTestRunner.cpp).
 */

#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonTools/ScreenEvents/ScreenEvents_Tests.h"
#include "CommonTools/ScreenMachine/ScreenMachine_Tests.h"
#include "Pokemon/AutoStory/Pokemon_AutoStory_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryCatchPlan_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryCatchStop_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryLines_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMarkerArrival_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMenuSave_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryZone1Map_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMinimapMatch_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryHeading_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackups_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBattleOnScreen_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryChainStart_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryGame_Tests.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneGolden_Tests.h"

namespace PokemonAutomation{
namespace{


UnitTestDatabase make_logic_tests(){
    UnitTestDatabase database;
    Pokemon::add_tests_AutoStory(database);
    add_tests_ScreenEvents(database);
    add_tests_ScreenMachine(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryHeading(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryBackups(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryBattleOnScreen(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryChainStart(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryCatchPlan(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryCatchStop(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryGame(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryLines(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryMarkerArrival(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryMenuSave(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryMinimapMatch(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStorySceneGolden(database);
    NintendoSwitch::PokemonLZA::add_tests_AutoStoryZone1Map(database);
    return database;
}


class StdoutLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string& msg, Color) override{
        std::cout << msg << std::endl;
    }
};


struct RunCounts{
    size_t passed = 0;
    size_t failed = 0;
    size_t skipped = 0;
};

UnitTestResult run_one(const UnitTest& test, Logger& logger){
    CancellableHolder<CancellableScope> scope;
    try{
        return test.run(logger, scope);
    }catch (const Exception& e){
        return UnitTestResult(UnitTestResult::FAILED, std::format("Exception: {}", e.to_str()));
    }catch (const std::exception& e){
        return UnitTestResult(UnitTestResult::FAILED, std::format("std::exception: {}", e.what()));
    }
}

void record(const UnitTest& test, const UnitTestResult& result, RunCounts& counts){
    switch (result.result){
    case UnitTestResult::PASSED:
        std::cout << "PASSED: " << test.name() << std::endl;
        counts.passed++;
        return;
    case UnitTestResult::NOT_RUN:
    case UnitTestResult::SKIPPED:
        std::cout << "SKIPPED: " << test.name() << std::endl;
        counts.skipped++;
        return;
    case UnitTestResult::FAILED:
    case UnitTestResult::OOM:
        std::cout << "FAILED: " << test.name() << ": " << result.message << std::endl;
        counts.failed++;
        return;
    }
}

int run_logic_tests(std::string_view filter){
    StdoutLogger logger;
    RunCounts counts;
    for (const auto& item : make_logic_tests()){
        if (!filter.empty() && !item.first.contains(filter)){
            continue;
        }
        record(*item.second, run_one(*item.second, logger), counts);
    }
    std::cout << "Tests Finished:"
              << "\n    Passed:  " << counts.passed
              << "\n    Failed:  " << counts.failed
              << "\n    Skipped: " << counts.skipped << std::endl;
    if (counts.passed + counts.failed == 0){
        std::cout << "No test ran." << std::endl;
        return 1;
    }
    return counts.failed > 0 ? 1 : 0;
}


}  // namespace
}  // namespace PokemonAutomation


int main(int argc, char* argv[]){
    std::string_view filter;
    for (int c = 1; c < argc; c++){
        std::string_view arg = argv[c];
        if (arg == "--filter" && c + 1 < argc){
            filter = argv[++c];
        }else{
            std::cerr << "Usage: " << argv[0] << " [--filter <substring>]" << std::endl;
            return 2;
        }
    }
    return PokemonAutomation::run_logic_tests(filter);
}
