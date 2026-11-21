/*  PokemonLZA AutoStory - Screen events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/ScreenEvents/ScreenFades.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogChoiceBox.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  run_towards_gate_with_A_button()'s box cut 0.05 shorter at the bottom.
const ImageFloatBox INTERACT_PROMPT_BOX{0.3, 0.2, 0.4, 0.65};

} // namespace


PlzaScreenObserver::PlzaScreenObserver()
    : m_dialog({.white = WhiteDialogPart::BOX_ONLY})
    , m_choice_arrow(COLOR_YELLOW, nullptr, SelectionArrowType::RIGHT, dialog_choice_box())
    , m_interact(COLOR_RED, ButtonType::ButtonA, INTERACT_PROMPT_BOX)
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
    const bool box = m_dialog.detect(screen);
    out.set(ScreenEvent::DIALOG_BOX, box);
    //  Choice menus without a visible selection arrow still show their highlighted row (console B 2026-11-19).
    out.set(ScreenEvent::CHOICE_BOX, m_choice_arrow.detect(screen) || (box && read_dialog_choice_rows(screen).has_value()));
    out.set(ScreenEvent::INTERACT_PROMPT, !box && m_interact.detect(screen));
    return out;
}

void PlzaScreenObserver::make_overlays(VideoOverlaySet& items) const{
    m_dialog.make_overlays(items);
    m_choice_arrow.make_overlays(items);
    m_interact.make_overlays(items);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
