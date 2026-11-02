/*  System Keyboard Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Switch's own software keyboard, which games open for names (e.g. the
 *  player's name in BDSP): detect it, find the key under its cursor, and type
 *  a string by moving the cursor and checking where it landed.
 *
 *  Measured on 1920x1080 captures of the QWERTY layout.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_SystemKeyboardDetector_H
#define PokemonAutomation_NintendoSwitch_SystemKeyboardDetector_H

#include <string>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "CommonTools/VisualDetector.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  White key panel and bottom bar, and the blue OK button.
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

//  Types `text` (letters, digits and the symbols on the QWERTY grid). For each character,
//  reads the cursor, moves it with the D-pad and checks it arrived before pressing A; an
//  uppercase letter is typed with Shift (left stick click). Throws, with a screenshot, if
//  the cursor can't be read or doesn't arrive.
void type_on_system_keyboard(VideoStream& stream, ProControllerContext& context, const std::string& text);



}
}
#endif
