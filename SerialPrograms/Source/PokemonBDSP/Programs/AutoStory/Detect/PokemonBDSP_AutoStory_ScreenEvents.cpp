/*  BDSP AutoStory - Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_ScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::ScreenEvent;
using Pokemon::ScreenEvents;


BdspScreenObserver::BdspScreenObserver(BattleCue cue, std::string banner)
    : m_cue(cue)
    , m_banner(std::move(banner))
    , m_wild_menu(BattleType::STANDARD)
    , m_trainer_menu(BattleType::TRAINER)
{}

ScreenEvents BdspScreenObserver::observe(const VideoSnapshot& frame, WallClock now){
    ScreenEvents out;
    if (!frame){
        m_previous = VideoSnapshot();
        return out;
    }
    const ImageViewRGB32& screen = *frame.frame;
    out.set(ScreenEvent::BATTLE_STARTED, m_wild_menu.detect(screen) || (m_cue == BattleCue::TRAINER &&
        (m_trainer_menu.detect(screen) || m_black.detect(screen))));
    out.set(ScreenEvent::DIALOG_BOX, m_dialog.detect(screen));
    out.set(ScreenEvent::PLAYER_CAN_MOVE, m_overworld.detect(screen));
    out.set(ScreenEvent::SCREEN_STILL, m_previous && walk_screen_change(*m_previous.frame, screen) < WALK_TAP_BLOCKED_CHANGE);
    out.add_fade(fade_of_screen(screen));
    out.set(ScreenEvent::LOCATION_BANNER, !m_banner.empty() && location_banner_matches(m_reads.read(screen, now), m_banner));
    m_previous = frame;
    return out;
}

void BdspScreenObserver::make_overlays(VideoOverlaySet& items) const{
    m_dialog.make_overlays(items);
    m_overworld.make_overlays(items);
    m_wild_menu.make_overlays(items);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
