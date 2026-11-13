/*  System Keyboard Typing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Types text on the Switch's on-screen keyboard with a Pro Controller, reading
 *  the cursor from the video before each key. Unlike keyboard_enter_code(),
 *  which moves blind from an assumed starting key, it starts wherever the
 *  keyboard left its cursor and corrects moves the keyboard ignored.
 *
 *  Supported keyboards are those of NintendoSwitch_SystemKeyboardDetector.h: the
 *  Latin keyboard, QWERTY or AZERTY. Console-tested on Switch 1 (2026-11-13):
 *  the player's name in Legends: Z-A in English, Spanish, German, Italian, French
 *  and Japanese, and earlier the player's and rival's names in BDSP. Not
 *  supported: the Korean and Chinese keyboards. Switch 2 is not tested.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_SystemKeyboardTyping_H
#define PokemonAutomation_NintendoSwitch_SystemKeyboardTyping_H

#include <string>
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_KeyboardEntryMappings.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace FastCodeEntry{


//  Types `text` on the open keyboard: letters, digits and the symbols on the
//  keyboard's first page. An uppercase letter is typed with Shift (left stick
//  click), which assumes the keyboard opened with Shift off, as BDSP's and
//  Legends: Z-A's do.
//
//  Before the first press, throws UserSetupError if `text` has a character with
//  no key. While typing, throws OperationFailedException, with a screenshot, if
//  the cursor can't be read or doesn't reach a key, or if a key press doesn't
//  add to the text field. The check counts the field's white pixels, so it
//  catches a dropped press but not a wrong case.
//
//  Presses are held for the "Fast Code Entry Timings" hold of Switch 1 with the
//  controller's connection type, as keyboard_enter_code() does. The waits after
//  them are fixed, since each move is read back from the video.
void type_on_system_keyboard(
    VideoStream& stream, ProControllerContext& context,
    const std::string& text,
    KeyboardLayout layout = KeyboardLayout::QWERTY
);


}  // namespace FastCodeEntry
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif
