/*  System Keyboard Typing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Types text on the Switch's on-screen keyboard with a Pro Controller, reading
 *  the cursor from the video before each key. Unlike keyboard_enter_code(),
 *  which moves blind from an assumed starting key, it works wherever the
 *  keyboard left its cursor and catches presses the keyboard ignored.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_SystemKeyboardTyping_H
#define PokemonAutomation_NintendoSwitch_SystemKeyboardTyping_H

#include <string>
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch_KeyboardEntryMappings.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace FastCodeEntry{


//  Types `text` (letters, digits and the symbols on the keyboard's first page)
//  on the open keyboard. An uppercase letter is typed with Shift (left stick
//  click). Throws, with a screenshot, for a character that has no key, or if
//  the cursor can't be read or doesn't reach a key.
void type_on_system_keyboard(
    VideoStream& stream, ProControllerContext& context,
    const std::string& text,
    KeyboardLayout layout = KeyboardLayout::QWERTY
);


}
}
}
#endif
