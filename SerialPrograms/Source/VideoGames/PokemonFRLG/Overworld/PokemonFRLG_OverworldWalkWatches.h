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
//  The same after a leg through grass. A battle whose flashes are missed is
//  seen only by its "Wild <Pokemon> appeared!", 3.7s after its step's press
//  (twice on Route 1, console B, 2026-11-01, run 7). Unless the walker waits
//  that long, the next leg is walked into the battle and lost: there, "east
//  4" and "west 4".
inline constexpr Milliseconds AFTER_WILD_LEG_WATCH(3500);
//  How long clear_open_message() watches for the message it saw.
inline constexpr Milliseconds OPEN_MESSAGE_WATCH(1000);
//  Maximum interruptions before a leg is abandoned.
inline constexpr size_t MAX_INTERRUPTIONS_PER_LEG = 5;

} // namespace OverworldWalkSteps



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
