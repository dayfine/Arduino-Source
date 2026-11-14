/*  PLZA AutoStory - Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/ScreenEvents/ScreenFades.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDialogChoiceBox.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  Where an interact prompt shows: over the scene, above the bottom hints
//  (the battle HUD's "X A Select Move" is lower).
//  This is run_towards_gate_with_A_button()'s box (PokemonLZA_BasicNavigation)
//  cut 0.05 shorter at the bottom, where upstream's reaches the battle HUD's
//  hints (BATTLE_HINT_BOX) and would read every battle frame as a prompt.
//  walk_to_prompt() reads this event through the story backend, rather than
//  calling run_towards_gate_with_A_button(), so the sim runs it too, and it
//  walks: the story's legs are timed at walking pace.
const ImageFloatBox INTERACT_PROMPT_BOX{0.3, 0.2, 0.4, 0.65};
//  The battle HUD's hints at the bottom centre: the A of "X A: Select Move"
//  (za2_hud_s3, za2_atk*: 15 of 15 battle frames, none of 442 others).
const ImageFloatBox BATTLE_HINT_BOX{0.40, 0.84, 0.20, 0.10};

} // namespace


bool SceneStart::never_started(const ScreenEvents& events, WallClock now){
    m_seen |= events.any_of({ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX, ScreenEvent::BATTLE_STARTED});
    return !m_seen && now >= m_deadline;
}

SceneMash::SceneMash(SceneMashGoal goal, bool text_button_is_b)
    : m_goal(goal)
    , m_text_button_is_b(text_button_is_b)
    , m_talk(Pokemon::TalkOverRule{.calm = OVERWORLD_HOLD})
{}

SceneMashAction SceneMash::look(const ScreenEvents& events, WallClock now){
    const Pokemon::SceneMove move = m_talk.look(events, now);
    switch (m_goal){
    case SceneMashGoal::CHOICE:
        if (events.has(ScreenEvent::CHOICE_BOX)){
            return SceneMashAction::DONE;
        }
        break;
    case SceneMashGoal::BATTLE:
        if (events.has(ScreenEvent::BATTLE_STARTED)){
            return SceneMashAction::DONE;
        }
        break;
    case SceneMashGoal::OVERWORLD:
        if (move == Pokemon::SceneMove::DONE){
            return SceneMashAction::DONE;
        }
        break;
    }
    if (events.has(ScreenEvent::FADE_BLACK) || events.has(ScreenEvent::FADE_WHITE)){
        return SceneMashAction::WAIT;
    }
    if (events.has(ScreenEvent::CHOICE_BOX)){
        return SceneMashAction::PRESS_A;
    }
    if (events.has(ScreenEvent::DIALOG_BOX)){
        return SceneMashAction::PRESS_TEXT;
    }
    if (m_goal == SceneMashGoal::BATTLE){
        return SceneMashAction::PRESS_A;
    }
    if (m_goal == SceneMashGoal::OVERWORLD && events.has(ScreenEvent::PLAYER_CAN_MOVE)){
        return SceneMashAction::WAIT;
    }
    return m_text_button_is_b ? SceneMashAction::PRESS_B : SceneMashAction::WAIT;
}


PlzaScreenObserver::PlzaScreenObserver()
    : m_dialog({.white = WhiteDialogPart::BOX_ONLY})
    , m_choice_arrow(COLOR_YELLOW, nullptr, SelectionArrowType::RIGHT, dialog_choice_box())
    , m_interact(COLOR_RED, ButtonType::ButtonA, INTERACT_PROMPT_BOX)
    , m_battle_hint(COLOR_CYAN, ButtonType::ButtonA, BATTLE_HINT_BOX)
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
        //  A uniform screen has no box, prompt or HUD to read.
        return out;
    }
    const bool box = m_dialog.detect(screen);
    out.set(ScreenEvent::DIALOG_BOX, box);
    out.set(ScreenEvent::CHOICE_BOX, m_choice_arrow.detect(screen));
    out.set(ScreenEvent::INTERACT_PROMPT, !box && m_interact.detect(screen));
    out.set(ScreenEvent::BATTLE_STARTED, m_battle_hint.detect(screen));
    out.set(ScreenEvent::PLAYER_CAN_MOVE, !box && m_party.detect(screen));
    return out;
}

void PlzaScreenObserver::make_overlays(VideoOverlaySet& items) const{
    m_dialog.make_overlays(items);
    m_choice_arrow.make_overlays(items);
    m_interact.make_overlays(items);
    m_battle_hint.make_overlays(items);
    m_party.make_overlays(items);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
