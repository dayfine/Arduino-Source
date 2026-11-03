/*  Naming Screen Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Detects the in-game keyboard used to name the player, the rival, boxes and Pokemon.
 *
 *  The boxes and colors come from pret/pokefirered src/naming_screen.c (commit
 *  ce6406d8c2eaf46d543c8a2606de0c3cb1951982), not from screenshots: the keyboard window
 *  covers tiles (3, 10)-(22, 18) and is filled with keyboard.pal color 13, 14 or 15 by page,
 *  and the text entry window at tiles (8, 6)-(22, 8) is white.
 *  Not yet checked against a capture.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_NamingScreenDetector_H
#define PokemonAutomation_PokemonFRLG_NamingScreenDetector_H

#include <chrono>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class NamingScreenDetector : public StaticScreenDetector{
public:
    NamingScreenDetector(Color color = COLOR_RED);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    //  The keyboard's margins left and right of the keys, clear of the cursor.
    ImageFloatBox m_keyboard_left_box;
    ImageFloatBox m_keyboard_right_box;
    //  The text entry window, right of the last character.
    ImageFloatBox m_entry_box;
};
class NamingScreenWatcher : public DetectorToFinder<NamingScreenDetector>{
public:
    NamingScreenWatcher(Color color = COLOR_RED)
        : DetectorToFinder("NamingScreenWatcher", std::chrono::milliseconds(250), color)
    {}
};


}
}
}

#endif
