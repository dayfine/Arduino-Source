/*  Nintendo Switch: Pro Controller to GBA keys (MgbaLink)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How a Pro Controller state drives an mGBA instance: A, B, L and R as
 *  themselves, Plus as Start, Minus as Select, the D-pad as the D-pad, and
 *  the left stick as the D-pad past half tilt on each axis. Everything else
 *  has no GBA key; the controller logs it.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_MgbaLink_KeyMap_H
#define PokemonAutomation_NintendoSwitch_MgbaLink_KeyMap_H

#include <stdint.h>
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerWithScheduler.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  The GBA keys one Pro Controller state holds, and what it presses that
//  has no GBA key.
struct GbaKeysFromSwitch{
    uint16_t keys = 0;                  //  MgbaLink::GbaKey bits.
    Button unmapped = BUTTON_NONE;      //  Pressed buttons with no GBA key.
    bool right_stick_moved = false;     //  The right stick has no GBA key.
};

//  Maps one state; the left stick adds to the D-pad, never cancels it.
GbaKeysFromSwitch gba_keys_from_switch(const SwitchControllerState& state);


}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif
