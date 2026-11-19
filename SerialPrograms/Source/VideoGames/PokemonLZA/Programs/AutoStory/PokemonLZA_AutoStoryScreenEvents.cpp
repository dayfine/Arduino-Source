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

//  run_towards_gate_with_A_button()'s box cut 0.05 shorter at the bottom,
//  where it would reach the battle HUD's hints (BATTLE_HINT_BOX).
const ImageFloatBox INTERACT_PROMPT_BOX{0.3, 0.2, 0.4, 0.65};
//  The A of "X A: Select Move" at the bottom centre (read on 15 of 15
//  battle frames, none of 442 others).
const ImageFloatBox BATTLE_HINT_BOX{0.40, 0.84, 0.20, 0.10};

} // namespace


PlzaScreenObserver::PlzaScreenObserver()
    : m_choice_arrow(COLOR_YELLOW, nullptr, SelectionArrowType::RIGHT, dialog_choice_box())
    , m_interact(COLOR_RED, ButtonType::ButtonA, INTERACT_PROMPT_BOX)
    , m_battle_hint(COLOR_CYAN, ButtonType::ButtonA, BATTLE_HINT_BOX)
    , m_battle_panel(COLOR_GREEN)
    , m_battle_markers(COLOR_BLUE)
    , m_party(COLOR_WHITE)
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
    out.set(ScreenEvent::CHOICE_BOX, m_choice_arrow.detect(screen));
    out.set(ScreenEvent::INTERACT_PROMPT, m_interact.detect(screen));
    out.set(ScreenEvent::BATTLE_STARTED, m_battle_hint.detect(screen) || (m_battle_panel.detect(screen) && m_battle_markers.detect(screen)));
    out.set(ScreenEvent::PLAYER_CAN_MOVE, m_party.detect(screen));
    return out;
}

void PlzaScreenObserver::make_overlays(VideoOverlaySet& items) const{
    m_choice_arrow.make_overlays(items);
    m_interact.make_overlays(items);
    m_battle_hint.make_overlays(items);
    m_battle_panel.make_overlays(items);
    m_battle_markers.make_overlays(items);
    m_party.make_overlays(items);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
