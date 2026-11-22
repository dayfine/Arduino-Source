/*  Nintendo Switch: Pro Controller to GBA keys (MgbaLink)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Controllers/MgbaLink/MgbaLink_Keys.h"
#include "NintendoSwitch_MgbaLink_KeyMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

using namespace MgbaLink;


namespace{

struct ButtonKey{
    Button button;
    uint16_t key;
};

constexpr ButtonKey BUTTON_KEYS[] = {
    {BUTTON_A,      GBA_KEY_A},
    {BUTTON_B,      GBA_KEY_B},
    {BUTTON_L,      GBA_KEY_L},
    {BUTTON_R,      GBA_KEY_R},
    {BUTTON_PLUS,   GBA_KEY_START},
    {BUTTON_MINUS,  GBA_KEY_SELECT},
    {BUTTON_UP,     GBA_KEY_UP},
    {BUTTON_RIGHT,  GBA_KEY_RIGHT},
    {BUTTON_DOWN,   GBA_KEY_DOWN},
    {BUTTON_LEFT,   GBA_KEY_LEFT},
};

//  Stick tilt past which an axis presses its direction.
constexpr double STICK_THRESHOLD = 0.5;

uint16_t dpad_keys(DpadPosition dpad){
    switch (dpad){
    case DPAD_UP:           return GBA_KEY_UP;
    case DPAD_UP_RIGHT:     return GBA_KEY_UP | GBA_KEY_RIGHT;
    case DPAD_RIGHT:        return GBA_KEY_RIGHT;
    case DPAD_DOWN_RIGHT:   return GBA_KEY_DOWN | GBA_KEY_RIGHT;
    case DPAD_DOWN:         return GBA_KEY_DOWN;
    case DPAD_DOWN_LEFT:    return GBA_KEY_DOWN | GBA_KEY_LEFT;
    case DPAD_LEFT:         return GBA_KEY_LEFT;
    case DPAD_UP_LEFT:      return GBA_KEY_UP | GBA_KEY_LEFT;
    case DPAD_NONE:         return GBA_KEY_NONE;
    }
    return GBA_KEY_NONE;
}

//  y > 0 is up, as everywhere in the controller code.
uint16_t stick_keys(const JoystickPosition& stick){
    uint16_t keys = GBA_KEY_NONE;
    if (stick.x > STICK_THRESHOLD){
        keys |= GBA_KEY_RIGHT;
    }else if (stick.x < -STICK_THRESHOLD){
        keys |= GBA_KEY_LEFT;
    }
    if (stick.y > STICK_THRESHOLD){
        keys |= GBA_KEY_UP;
    }else if (stick.y < -STICK_THRESHOLD){
        keys |= GBA_KEY_DOWN;
    }
    return keys;
}

}  // namespace



GbaKeysFromSwitch gba_keys_from_switch(const SwitchControllerState& state){
    GbaKeysFromSwitch result;
    Button mapped = BUTTON_NONE;
    for (const ButtonKey& entry : BUTTON_KEYS){
        mapped |= entry.button;
        if ((state.buttons & entry.button) != BUTTON_NONE){
            result.keys |= entry.key;
        }
    }
    result.unmapped = state.buttons & (Button)~(ButtonFlagType)mapped;
    result.keys |= dpad_keys(state.dpad);
    result.keys |= stick_keys(state.left_joystick);
    result.right_stick_moved = !state.right_joystick.is_neutral();
    return result;
}


}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
