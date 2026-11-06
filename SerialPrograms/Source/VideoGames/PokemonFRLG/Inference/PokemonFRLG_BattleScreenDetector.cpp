/*  Battle Screen Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstddef>
#include <cstdint>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_BattleScreenDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


BattleScreenDetector::BattleScreenDetector(Color color)
    : m_dialog(color)
    , m_menu(color)
    , m_opponent(color)
    , m_player(color)
{}
void BattleScreenDetector::make_overlays(VideoOverlaySet& items) const{
    m_dialog.make_overlays(items);
    m_opponent.make_overlays(items);
    m_player.make_overlays(items);
}
bool BattleScreenDetector::detect(const ImageViewRGB32& screen){
    if (m_menu.detect(screen)){
        return true;
    }
    return m_dialog.detect(screen) && (m_opponent.detect(screen) || m_player.detect(screen));
}


namespace{

//  On the 240x160 screen: the battle YES/NO box's left frame is 2 pixels of
//  slate blue at x 187-188, from y 66 to 109 (console B, 2026-11-03,
//  "Delete a move to make room for BITE?"). This box takes 3 pixels either
//  side of it, over the rows clear of its corners.
const ImageFloatBox YES_NO_FRAME_BOX(184.0 / 240, 70.0 / 160, 8.0 / 240, 34.0 / 160);
//  The rows with frame in them, of the box's: nearly all of them.
constexpr double FRAME_ROWS = 0.9;
//  Pixels of frame a row needs: about a third of a game pixel.
constexpr size_t FRAME_PIXELS = 2;

//  The frame's slate blue, measured (120, 115, 145) with its lighter edge
//  (147, 155, 218); not the empty HP bar's dark gray (75, 74, 87), the
//  white or the HP box's cream.
bool frame_color(uint32_t pixel){
    const int r = (pixel >> 16) & 0xff;
    const int g = (pixel >> 8) & 0xff;
    const int b = pixel & 0xff;
    return r >= 95 && r <= 170 && g >= 90 && g <= 175 && b >= 120 && b <= 240 && b >= r + 15;
}

} // namespace

bool battle_yes_no_shows(const ImageViewRGB32& screen){
    if (!BattleLearnDialogDetector(COLOR_RED).detect(screen)){
        return false;
    }
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const ImageViewRGB32 frame = extract_box_reference(game, YES_NO_FRAME_BOX);
    size_t rows = 0;
    for (size_t y = 0; y < frame.height(); y++){
        size_t pixels = 0;
        for (size_t x = 0; x < frame.width(); x++){
            pixels += frame_color(frame.pixel(x, y)) ? 1 : 0;
        }
        rows += pixels >= FRAME_PIXELS ? 1 : 0;
    }
    return frame.height() > 0 && rows >= FRAME_ROWS * frame.height();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
