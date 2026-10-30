/*  Naming Screen Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/FloatPixel.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_NamingScreenDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Keyboard fill colors by page (keyboard.pal 13, 14, 15) as channel ratios:
//  (123, 172, 197), (213, 156, 115), (148, 189, 106).
const FloatPixel KEYBOARD_COLORS[] = {
    {0.250, 0.350, 0.400},  //  upper case
    {0.440, 0.322, 0.238},  //  lower case
    {0.334, 0.427, 0.239},  //  symbols
};

bool is_keyboard_color(const ImageViewRGB32& image, const FloatPixel& color){
    return is_solid(image, color, 0.08, 30);
}

}


//  Boxes are fractions of the 240x160 GBA screen inside the game box.
NamingScreenDetector::NamingScreenDetector(Color color)
    : m_color(color)
    , m_keyboard_left_box(25.0 / 240, 82.0 / 160, 4.0 / 240, 60.0 / 160)      //  x 25-29, y 82-142
    , m_keyboard_right_box(170.0 / 240, 82.0 / 160, 5.0 / 240, 60.0 / 160)    //  x 170-175, y 82-142
    , m_entry_box(168.0 / 240, 50.0 / 160, 6.0 / 240, 12.0 / 160)             //  x 168-174, y 50-62
{}
void NamingScreenDetector::make_overlays(VideoOverlaySet& items) const{
    const BoxOption& GAME_BOX = GameSettings::instance().GAME_BOX;
    items.add(m_color, GAME_BOX.inner_to_outer(m_keyboard_left_box));
    items.add(m_color, GAME_BOX.inner_to_outer(m_keyboard_right_box));
    items.add(m_color, GAME_BOX.inner_to_outer(m_entry_box));
}
bool NamingScreenDetector::detect(const ImageViewRGB32& screen){
    ImageViewRGB32 game_screen = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    ImageViewRGB32 left = extract_box_reference(game_screen, m_keyboard_left_box);
    ImageViewRGB32 right = extract_box_reference(game_screen, m_keyboard_right_box);
    if (!is_white(extract_box_reference(game_screen, m_entry_box))){
        return false;
    }
    for (const FloatPixel& color : KEYBOARD_COLORS){
        if (is_keyboard_color(left, color) && is_keyboard_color(right, color)){
            return true;
        }
    }
    return false;
}


}
}
}
