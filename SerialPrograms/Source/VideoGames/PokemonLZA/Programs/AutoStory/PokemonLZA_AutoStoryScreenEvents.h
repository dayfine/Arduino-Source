/*  PokemonLZA AutoStory - Screen events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's detectors as the shared screen events
 *  (CommonTools/ScreenEvents/ScreenEvents.h), for the story backend.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_H
#define PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_H

#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_ButtonDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_OverworldPartySelectionDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryBattlePanelDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The shared events from PLZA's detectors:
//  - FADE_BLACK / FADE_WHITE: fade_of_screen();
//  - DIALOG_BOX: a white dialogue box's bottom edge (StoryDialogDetector's
//    BOX_ONLY: the box shows while the text still types), or a blue or
//    teal box;
//  - CHOICE_BOX: the selection arrow where a dialogue's choices are drawn
//    (dialog_choice_box());
//  - INTERACT_PROMPT: an A prompt over the scene ("A Talk", "A Leave"),
//    with no box;
//  - PLAYER_CAN_MOVE: the party bar at the bottom left
//    (OverworldPartySelectionDetector), with no box. Hotel Z's lobby,
//    halls and rooms do not show it;
//  - BATTLE_STARTED: the battle HUD's hints at the bottom centre ("X A:
//    Select Move"), read by their A icon, or the player's Pokemon's panel
//    at the bottom left (BattlePanelDetector): the hints show only while
//    the player is locked on, and Taunie's battle (03.02b) ran 70 s with
//    none (console B, 2026-11-16). Story battles show no Run prompt, so
//    RunFromBattleDetector misses them.
//  A fading screen has no box to read.
class PlzaScreenObserver : public ScreenEventObserver{
public:
    PlzaScreenObserver();

    virtual ScreenEvents observe(const VideoSnapshot& frame, WallClock now) override;

    //  observe() for a frame that is not a snapshot.
    ScreenEvents read(const ImageViewRGB32& screen);

    virtual void make_overlays(VideoOverlaySet& items) const override;

private:
    StoryDialogDetector m_dialog;
    SelectionArrowDetector m_choice_arrow;
    ButtonDetector m_interact;
    ButtonDetector m_battle_hint;
    BattlePanelDetector m_battle_panel;
    OverworldPartySelectionDetector m_party;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
