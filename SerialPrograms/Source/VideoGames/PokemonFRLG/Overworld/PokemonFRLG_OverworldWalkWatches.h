/*  Overworld Walk Watches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How long the walker watches for a battle or a message after a warp
 *  or a leg, and how often a leg may be interrupted.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWalkWatches_H
#define PokemonAutomation_PokemonFRLG_OverworldWalkWatches_H

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_LegWalker.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldWalkTimes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace OverworldWalkSteps{

//  After a warp's fade, which a wild battle's flashes and wipe can pass for:
//  a battle's HP box shows within this long (console B, 2026-11-01).
inline constexpr Milliseconds AFTER_WARP_BATTLE_WATCH(5000);
//  A message set off by a leg's last step shows after the hold ends. Caught
//  here, before the next leg's turn tap, which the message would swallow.
inline constexpr Milliseconds AFTER_LEG_WATCH(500);
//  A wild battle whose flashes are missed is seen only by its "Wild
//  <Pokemon> appeared!": the flashes, the wipe and the opening animation
//  take 3.7s from its step's press, twice on Route 1 (console B, 2026-11-01,
//  run 7). Other maps open battles with other wipes, so elsewhere this is a
//  guess. Every interrupt for such a battle places the stop with this.
inline constexpr Milliseconds LATE_WILD_BATTLE_DELAY(3700);
//  The same after a leg through grass, for a battle seen that late. Unless
//  the walker waits that long, the next leg is walked into the battle and
//  lost: on Route 1, "east 4" and "west 4".
inline constexpr Milliseconds AFTER_WILD_LEG_WATCH(3500);
//  After a leg's watch saw nothing, but the screen shows a battle on its
//  way: how long to watch on for its HP boxes. In Mt. Moon they came over
//  5s after the step that set the battle off, after the watch had ended on
//  the battle's empty text box (console B, 2026-11-03, row 00.13).
inline constexpr Milliseconds UNSEEN_BATTLE_WATCH(5000);
//  How long clear_open_message() watches for the message it saw.
inline constexpr Milliseconds OPEN_MESSAGE_WATCH(1000);
//  Maximum interruptions before a leg is abandoned.
inline constexpr size_t MAX_INTERRUPTIONS_PER_LEG = 5;

} // namespace OverworldWalkSteps



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
