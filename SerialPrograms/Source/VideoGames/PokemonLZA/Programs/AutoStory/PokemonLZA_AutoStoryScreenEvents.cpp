/*  PokemonLZA AutoStory - Screen events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/ScreenEvents/ScreenFades.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDialogChoiceBox.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The battle HUD's hints at the bottom centre: the A of "X A: Select Move"
//  (lab-B frames za2_hud_s3, za2_atk*: 15 of 15 battle frames, none of 442
//  others, 2026-11-08).
const ImageFloatBox BATTLE_HINT_BOX{0.40, 0.84, 0.20, 0.10};

} // namespace


PlzaScreenObserver::PlzaScreenObserver()
    : m_dialog({.white = WhiteDialogPart::BOX_ONLY})
    , m_choice_arrow(COLOR_YELLOW, nullptr, SelectionArrowType::RIGHT, dialog_choice_box())
    , m_battle_hint(COLOR_CYAN, ButtonType::ButtonA, BATTLE_HINT_BOX)
{}

ScreenEvents PlzaScreenObserver::observe(const VideoSnapshot& frame, WallClock){
    if (!frame){
        return {};
    }
    return read(*frame.frame);
}

ScreenEvents PlzaScreenObserver::read(const ImageViewRGB32& screen){
    ScreenEvents out = fade_events(fade_of_screen(screen));
    if (fade_in(out) != WarpFade::NONE){
        return out;
    }
    out.set(ScreenEvent::DIALOG_BOX, m_dialog.detect(screen));
    out.set(ScreenEvent::CHOICE_BOX, m_choice_arrow.detect(screen));
    out.set(ScreenEvent::BATTLE_STARTED, m_battle_hint.detect(screen));
    return out;
}

void PlzaScreenObserver::make_overlays(VideoOverlaySet& items) const{
    m_dialog.make_overlays(items);
    m_choice_arrow.make_overlays(items);
    m_battle_hint.make_overlays(items);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
