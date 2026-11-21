/*  PokemonLZA AutoStory - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStory_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStory_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Pure logic suites (Qt/OpenCV-free, run in SerialProgramsLogicTests too).
void add_tests_AutoStoryActions(UnitTestDatabase& database);
void add_tests_AutoStoryGame(UnitTestDatabase& database);
void add_tests_AutoStoryMinimapMatch(UnitTestDatabase& database);

//  Full-build suites (image/screenshot detectors and new-game validation).
void add_tests_AutoStoryDetectors(UnitTestDatabase& database);
void add_tests_AutoStoryDialogDetectors(UnitTestDatabase& database);
void add_tests_AutoStoryIntroScreens(UnitTestDatabase& database);
void add_tests_AutoStoryNewGame(UnitTestDatabase& database);
void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database);

inline void add_tests_AutoStoryLogic(UnitTestDatabase& database){
    add_tests_AutoStoryActions(database);
    add_tests_AutoStoryGame(database);
    add_tests_AutoStoryMinimapMatch(database);
}

inline void add_tests_AutoStoryImage(UnitTestDatabase& database){
    add_tests_AutoStoryDetectors(database);
    add_tests_AutoStoryDialogDetectors(database);
    add_tests_AutoStoryIntroScreens(database);
    add_tests_AutoStoryNewGame(database);
    add_tests_AutoStoryScreenEvents(database);
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
