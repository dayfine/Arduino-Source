/*  BDSP AutoStory - Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkScreen.h"
#include "PokemonBDSP_AutoStory_ScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::ScreenEvent;
using Pokemon::ScreenEvents;

namespace{
//  Around the player, whom the camera keeps in the middle: PLAYER_STILL's
//  box. People walking elsewhere on the screen stay out of it.
const ImageFloatBox PLAYER_BOX(0.42, 0.34, 0.16, 0.32);
}


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
    out.set(ScreenEvent::PLAYER_STILL, m_previous && walk_screen_change(
        extract_box_reference(*m_previous.frame, PLAYER_BOX), extract_box_reference(screen, PLAYER_BOX)) < WALK_TAP_BLOCKED_CHANGE);

    out.merge(Pokemon::fade_events(fade_of_screen(screen)));
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
