/*  AutoStory Check Level Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's rows at a check level on the story maps' simulation: a check
 *  step the level doesn't run is skipped, not played or stubbed.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCheckLevel_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCheckLevel_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Rows 17.04 and 17.04b on StoryMapsSim at the Default and Thorough check levels.
void add_tests_AutoStoryCheckLevel(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
