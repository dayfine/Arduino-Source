/*  BDSP AutoStory Segment 00 Steps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Segment00Steps_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Segment00Steps_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Offline checks of 00.04b as story steps: it lints clean, and every route
//  plans on the story maps from where the step before leaves the player.
void add_tests_AutoStory_Segment00Steps(UnitTestDatabase& database);



}
}
}
#endif
