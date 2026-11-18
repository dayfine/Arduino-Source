/*  FRLG AutoStory - Frame Replay Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFrameReplay_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFrameReplay_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Console failures replayed from their recorded frames
//  (CommandLineTests/PokemonFRLG/FrameReplay/) through FRLG's detectors.
void add_tests_AutoStoryFrameReplay(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
