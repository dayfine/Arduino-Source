/*  System Keyboard Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Switch's own on-screen keyboard, which games open for names (e.g. the
 *  player's name in BDSP): detect it, find the key under its cursor, and tell
 *  whether its text field is empty. Typing with it is in
 *  Programs/FastCodeEntry/NintendoSwitch_SystemKeyboardTyping.h.
 *
 *  Measured on 1920x1080 captures of the QWERTY layout.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_SystemKeyboardDetector_H
#define PokemonAutomation_NintendoSwitch_SystemKeyboardDetector_H

#include <string>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  White key panel and bottom bar, and the OK button (blue, or grey while the
//  field is empty).
class SystemKeyboardDetector : public StaticScreenDetector{
public:
    SystemKeyboardDetector(Color color = COLOR_CYAN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_key_panel;
    ImageFloatBox m_bottom_bar;
    ImageFloatBox m_ok_button;
};
class SystemKeyboardWatcher : public DetectorToFinder<SystemKeyboardDetector>{
public:
    SystemKeyboardWatcher(Color color = COLOR_CYAN)
        : DetectorToFinder("SystemKeyboardWatcher", std::chrono::milliseconds(250), color)
    {}
};


//  The key under the cursor, as row (0: digits .. 3: "zxcv") and column (0..10).
//  The keyboard remembers the cursor between uses, so read it, don't assume it.
//  Returns false if no cursor border is visible.
bool find_system_keyboard_cursor(const ImageViewRGB32& screen, int& row, int& col);

//  Whether the text field holds any text. Pressing B on an empty field closes the
//  keyboard, so delete only while this is true.
bool system_keyboard_field_has_text(const ImageViewRGB32& screen);



void add_tests_SystemKeyboardDetector(UnitTestDatabase& database);



}
}
#endif
