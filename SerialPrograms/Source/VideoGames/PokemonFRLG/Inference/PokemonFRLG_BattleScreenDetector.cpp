/*  Battle Screen Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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



}
}
}
