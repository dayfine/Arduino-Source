/*  BDSP AutoStory - Actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Actions_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Actions_Tests_H

#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The name and body of a test.
struct AutoStoryActionsInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

//  What a test expects: its body checks it.
struct AutoStoryActionsExpectation{
};

//  One test of add_tests_AutoStory_Actions().
struct AutoStoryActionsCase{
    AutoStoryActionsInput input;
    AutoStoryActionsExpectation expectation;
};

//  Tests that need no console: the new game's intro decisions, and the screenshot checks of the Pokemon Center, the save card and the School's lamp.
void add_tests_AutoStory_Actions(UnitTestDatabase& database);

//  The battle tests (BattleActions_Tests.cpp): moves, Potions, the gym
//  opponent and the badge check. add_tests_AutoStory_Actions() adds them.
std::vector<AutoStoryActionsCase> battle_action_cases();



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
