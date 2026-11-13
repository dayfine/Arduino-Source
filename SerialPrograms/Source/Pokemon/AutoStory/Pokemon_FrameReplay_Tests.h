/*  Frame Replay Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_FrameReplay_Tests_H
#define PokemonAutomation_Pokemon_FrameReplay_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Recorded frames replayed through the detectors on a virtual clock.
void add_tests_FrameReplay(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
