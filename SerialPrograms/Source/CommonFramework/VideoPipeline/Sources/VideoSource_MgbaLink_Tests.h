/*  Video Source (mGBA over MgbaLink) Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_VideoPipeline_VideoSource_MgbaLink_Tests_H
#define PokemonAutomation_VideoPipeline_VideoSource_MgbaLink_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{


//  Compose geometry (nearest-neighbour into GAME_BOX) and address parsing.
void add_tests_VideoSource_MgbaLink(UnitTestDatabase& database);


}
#endif
