/*  System Keyboard Typing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Types text on the Switch's on-screen keyboard, reading the cursor from the video
 *  before each key, so it starts wherever the cursor is and corrects ignored moves.
 *  Supports the keyboards of NintendoSwitch_SystemKeyboardDetector.h.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_SystemKeyboardTyping_H
#define PokemonAutomation_NintendoSwitch_SystemKeyboardTyping_H

#include <cstddef>
#include <functional>
#include <string>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_KeyboardEntryMappings.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace FastCodeEntry{


//  Navigates the cursor to the bottom-row English/Latin key (row 4, col 3) and
//  presses A so CJK IME keyboards (e.g. Simplified Chinese Pinyin) switch to
//  Latin letters before typing.
//  Throws OperationFailedException if the cursor cannot reach that key.
void switch_system_keyboard_to_letters(
    VideoStream& stream, ProControllerContext& context
);

//  Types letters, digits and first-page symbols; uppercase via Shift (left stick
//  click), assuming the keyboard opens with Shift off.
//  Throws UserSetupError before the first press if a character has no key, and
//  OperationFailedException if the cursor can't be read or a press doesn't add to the
//  field (a wrong case is not caught). Holds are the Switch 1 code entry hold.
void type_on_system_keyboard(
    VideoStream& stream, ProControllerContext& context,
    const std::string& text,
    KeyboardLayout layout = KeyboardLayout::QWERTY
);

//  Presses PLUS up to `max_presses` times until `is_closed` holds for consecutive
//  frames (default: no cursor border visible via read_system_keyboard_cursor()).
//  Retries cover CJK IME keyboards that consume the first PLUS to commit composing
//  text before a second PLUS presses OK.
//  Throws OperationFailedException if the keyboard stays open.
void confirm_system_keyboard(
    VideoStream& stream, ProControllerContext& context,
    const std::function<bool(const ImageViewRGB32&)>& is_closed = {},
    size_t max_presses = 3,
    Milliseconds wait_per_press = Milliseconds(2500)
);


//  Register the tests of this file.
void add_tests_SystemKeyboardTyping(UnitTestDatabase& database);


}  // namespace FastCodeEntry
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif
