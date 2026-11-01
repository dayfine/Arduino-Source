/*  Battle Flash Watcher
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_BattleFlashWatcher.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

//  On the console a flash took the screen from 145 to 98 (two thirds) and
//  back within 80ms, twice in about 300ms.
constexpr std::chrono::milliseconds FLASH_WINDOW = 400ms;
constexpr double FLASH_DARKER = 0.8;    //  the dip is below this much of the brightness before it
constexpr double FLASH_FLOOR = 0.4;     //  and above this much: lower is a fade to black
constexpr double FLASH_RECOVERY = 1.1;  //  then the screen comes back to this much of the dip

}


BattleFlashWatcher::BattleFlashWatcher(Color color)
    : VisualInferenceCallback("BattleFlashWatcher")
    , m_color(color)
{}

void BattleFlashWatcher::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, GameSettings::instance().GAME_BOX);
}

bool BattleFlashWatcher::process_frame(const ImageViewRGB32& frame, WallClock timestamp){
    const ImageViewRGB32 game_screen = extract_box_reference(frame, GameSettings::instance().GAME_BOX);
    const double brightness = image_stats(game_screen).average.sum();
    while (!m_history.empty() && timestamp - m_history.front().first > FLASH_WINDOW){
        m_history.pop_front();
    }

    //  The brightest frame before the screen first dips, and the darkest
    //  frame since.
    double before = 0;
    double dip = -1;
    for (const auto& item : m_history){
        if (dip < 0){
            if (item.second < before * FLASH_DARKER){
                dip = item.second;
            }else{
                before = std::max(before, item.second);
            }
        }else{
            dip = std::min(dip, item.second);
        }
    }
    m_history.emplace_back(timestamp, brightness);

    return dip >= 0
        && dip > before * FLASH_FLOOR
        && brightness > dip * FLASH_RECOVERY;
}



}
}
}
