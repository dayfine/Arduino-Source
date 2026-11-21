/*  Lock-On Sweep
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_LockOnSweep_H
#define PokemonAutomation_PokemonLZA_LockOnSweep_H

#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


// Sweep the camera to lock on to a wild pokemon: `rounds` rounds of turning the camera with
// the right stick at `camera_x` (negative turns left) for 1 s, D-pad up, then mashing ZL
// for 1 s and holding it for 0.5 s. Then press L to put the camera back behind the player.
// This only presses buttons. Run it with a MoveEffectivenessSymbolWatcher to stop at a lock-on.
void sweep_camera_for_lock_on(ProControllerContext& context, double camera_x, int rounds);

// Run sweep_camera_for_lock_on() until the battle menu's move effectiveness symbols show,
// which means ZL has locked on to a wild pokemon.
// Return true if locked on, false if the whole sweep ended without a lock-on.
bool lock_on_by_sweep(ConsoleHandle& console, ProControllerContext& context, double camera_x, int rounds);


}  // namespace PokemonLZA
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif
