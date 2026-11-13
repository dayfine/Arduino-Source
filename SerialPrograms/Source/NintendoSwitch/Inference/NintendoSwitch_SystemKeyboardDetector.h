/*  System Keyboard Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Switch's own on-screen keyboard, which games open for names (e.g. the
 *  player's name in BDSP and in Legends: Z-A): detect it, read the key under its
 *  cursor, and tell whether its text field is empty. Typing with it is in
 *  Programs/FastCodeEntry/NintendoSwitch_SystemKeyboardTyping.h.
 *
 *  Supported: the Latin keyboard (QWERTY; AZERTY shares the key grid but has no
 *  test screenshot yet). Its layout follows the console's system language, not
 *  the game's. Measured on 1920x1080 Switch 1 captures of BDSP's and Legends:
 *  Z-A's name entry, the latter with the game in English, Spanish, German,
 *  Italian, French and Japanese (all QWERTY on an English console). Not
 *  supported: the Korean keyboard, whose candidate strip
 *  moves the keys down (the detector returns false on it), and probably the
 *  Chinese ones (not tested). Switch 2 is not tested.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_SystemKeyboardDetector_H
#define PokemonAutomation_NintendoSwitch_SystemKeyboardDetector_H

#include <stdint.h>
#include <optional>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  The white key panel, the white bar below it, and the OK button (blue, or grey
//  while the field is empty). Works wherever the cursor is.
class SystemKeyboardDetector : public StaticScreenDetector{
public:
    SystemKeyboardDetector(Color color = COLOR_CYAN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    //  Two strips across the key panel, one over the gap between rows 0 and 1 and
    //  one over the gap between rows 2 and 3. The cursor's border reaches into at
    //  most one of them, so the panel is white if either is.
    ImageFloatBox m_key_panel_upper;
    ImageFloatBox m_key_panel_lower;
    ImageFloatBox m_bottom_bar;
    ImageFloatBox m_ok_button;
};

//  Finds SystemKeyboardDetector on the video, checking every 250 ms.
class SystemKeyboardWatcher : public DetectorToFinder<SystemKeyboardDetector>{
public:
    SystemKeyboardWatcher(Color color = COLOR_CYAN)
        : DetectorToFinder("SystemKeyboardWatcher", std::chrono::milliseconds(250), color)
    {}
};


//  A key of the keyboard.
//  Rows 0-3 are the character rows (0: digits .. 3: "zxcv"), row 4 is the bottom
//  row (globe, Shift, ABC, #+=, the accent pages and Space).
//  Columns 0-10 are the character columns. Column 11 is the right column, whose
//  keys are taller: backspace is (0, 11), Return (1, 11) and OK (3, 11).
//  A row-4 key reports the character column nearest its centre (Space: 8).
struct SystemKeyboardKey{
    uint8_t row;
    uint8_t col;

    bool operator==(const SystemKeyboardKey&) const = default;
};

//  The key under the cursor, or nothing if no cursor border is visible.
//  The keyboard remembers the cursor between uses, so read it, don't assume it.
std::optional<SystemKeyboardKey> read_system_keyboard_cursor(const ImageViewRGB32& screen);

//  Whether the text field holds any text: the backspace key is dark then and light
//  grey while the field is empty. Pressing B on an empty field closes the
//  keyboard, so delete only while this is true.
bool system_keyboard_field_has_text(const ImageViewRGB32& screen);

//  The number of white pixels in the text field. Each typed character adds some
//  (a "1" adds about 110 at 1920x1080), so comparing two counts tells whether a
//  key press reached the field.
size_t system_keyboard_text_pixels(const ImageViewRGB32& screen);



//  Register the tests of this file.
void add_tests_SystemKeyboardDetector(UnitTestDatabase& database);



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif
