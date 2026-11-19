/*  System Keyboard Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Switch's own on-screen keyboard (game name entry): detect it, read the key
 *  under its cursor, and tell whether its text field is empty.
 *
 *  Supported: the Latin keyboard (QWERTY; AZERTY untested), whose layout follows
 *  the system language. Korean is not supported (detector returns false); Chinese
 *  and Switch 2 are untested.
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


//  The key panel, the bar below it and the OK button, in the light or dark theme,
//  wherever the cursor is.
class SystemKeyboardDetector : public StaticScreenDetector{
public:
    SystemKeyboardDetector(Color color = COLOR_CYAN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    //  Strips over the row 0-1 and 2-3 gaps; the cursor's border reaches at most one.
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


//  Rows 0-3: character rows (0: digits); row 4: the bottom row. Columns 0-10:
//  characters; column 11: backspace (0, 11), Return (1, 11), OK (3, 11).
//  A row-4 key reports the nearest character column (Space: 8).
struct SystemKeyboardKey{
    uint8_t row;
    uint8_t col;

    bool operator==(const SystemKeyboardKey&) const = default;
};

//  The key under the cursor, or nothing if no cursor border is visible.
//  The keyboard remembers the cursor between uses, so read it, don't assume it.
std::optional<SystemKeyboardKey> read_system_keyboard_cursor(const ImageViewRGB32& screen);

//  The backspace key is dark with text, light grey without. B on an empty field
//  closes the keyboard, so delete only while this is true.
bool system_keyboard_field_has_text(const ImageViewRGB32& screen);

//  White pixels in the text field (a "1" adds about 110 at 1080p): compare two
//  counts to tell whether a key press reached the field.
size_t system_keyboard_text_pixels(const ImageViewRGB32& screen);



//  Register the tests of this file.
void add_tests_SystemKeyboardDetector(UnitTestDatabase& database);



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif
