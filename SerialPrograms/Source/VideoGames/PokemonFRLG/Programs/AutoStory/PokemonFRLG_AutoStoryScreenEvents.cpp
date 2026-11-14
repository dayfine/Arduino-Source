/*  FRLG AutoStory - Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "CommonTools/ScreenEvents/ScreenFades.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



FrlgScreenObserver::FrlgScreenObserver()
    : m_dialog(COLOR_RED)
    , m_choice(COLOR_RED)
    , m_battle_menu(COLOR_RED)
    , m_battle_dialog(COLOR_RED)
{}

ScreenEvents FrlgScreenObserver::observe(const VideoSnapshot& frame, WallClock){
    if (!frame){
        return {};
    }
    const ImageViewRGB32& screen = *frame.frame;
    if (OverworldWalkSteps::dark_cave_screen(screen)){
        //  Its black below passes for a battle's text box.
        return {};
    }
    ScreenEvents out = fade_events(fade_of_screen(screen));
    if (fade_in(out) != WarpFade::NONE){
        //  A uniform screen has no box or menu to read.
        return out;
    }
    const bool battle = m_battle_menu.detect(screen) || m_battle_dialog.detect(screen);
    out.set(ScreenEvent::BATTLE_STARTED, battle);
    out.set(ScreenEvent::DIALOG_BOX, !battle && m_dialog.detect(screen));
    out.set(ScreenEvent::CHOICE_BOX, !battle && m_choice.detect(screen));
    return out;
}

void FrlgScreenObserver::make_overlays(VideoOverlaySet& items) const{
    m_dialog.make_overlays(items);
    m_choice.make_overlays(items);
    m_battle_menu.make_overlays(items);
    m_battle_dialog.make_overlays(items);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
