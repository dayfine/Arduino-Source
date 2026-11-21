/*  Home Screen Observer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "NintendoSwitch_HomeScreenObserver.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


SwitchHomeObserver::SwitchHomeObserver(ConsoleHandle& console, ScreenEventObserver& game)
    : m_game(game)
    , m_home(console, COLOR_RED)
{}

ScreenEvents SwitchHomeObserver::observe(const VideoSnapshot& frame, WallClock now){
    if (frame && m_home.detect_only(*frame.frame)){
        return {ScreenEvent::SWITCH_HOME_MENU};
    }
    return m_game.observe(frame, now);
}

void SwitchHomeObserver::make_overlays(VideoOverlaySet& items) const{
    m_game.make_overlays(items);
}



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
