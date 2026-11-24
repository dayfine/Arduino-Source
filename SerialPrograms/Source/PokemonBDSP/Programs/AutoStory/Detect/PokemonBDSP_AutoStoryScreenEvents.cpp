/*  BDSP AutoStory - Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalkScreen.h"
#include "PokemonBDSP_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using PokemonAutomation::ScreenEvent;
using PokemonAutomation::ScreenEvents;

namespace{
//  Around the player, whom the camera keeps in the middle: PLAYER_STILL's
//  box. People walking elsewhere on the screen stay out of it.
const ImageFloatBox PLAYER_BOX(0.42, 0.34, 0.16, 0.32);
} // namespace


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
        m_player_still = false;
        m_wild_shown = false;
        return out;
    }
    const ImageViewRGB32& screen = *frame.frame;
    m_wild_shown = m_wild_menu.detect(screen);
    out.set(ScreenEvent::BATTLE_STARTED, m_wild_shown || m_trainer_menu.detect(screen) ||
        (m_cue == BattleCue::TRAINER && m_black.detect(screen)));
    out.set(ScreenEvent::DIALOG_BOX, m_dialog.detect(screen));
    out.set(ScreenEvent::PLAYER_CAN_MOVE, m_overworld.detect(screen));
    out.set(ScreenEvent::SCREEN_STILL, m_previous && walk_screen_change(*m_previous.frame, screen) < WALK_TAP_BLOCKED_CHANGE);
    m_player_still = m_previous && walk_screen_change(
        extract_box_reference(*m_previous.frame, PLAYER_BOX), extract_box_reference(screen, PLAYER_BOX)) < WALK_TAP_BLOCKED_CHANGE;

    out.merge(fade_events(fade_of_screen(screen)));
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
