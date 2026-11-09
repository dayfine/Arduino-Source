/*  Overworld Late Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a wild battle seen only after an open leg's hold stopped the
 *  player. The walker records the leg's screens as timed events
 *  (Pokemon::ScreenEventLog) through the hold and the watch after it. The
 *  shared classifier (Pokemon::classify_interrupt()) says what stopped the
 *  player, and a wild battle is placed by when its first timed event
 *  showed, not by when the watcher that cleared it fired: that is after the
 *  hold, and the leg could have stopped anywhere along it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldLateBattle_H
#define PokemonAutomation_PokemonFRLG_OverworldLateBattle_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Registers the late battle unit tests, on the sim.
void add_tests_OverworldLateBattle(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
