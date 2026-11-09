/*  PLZA AutoStory - Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's detectors as the shared screen events (Pokemon_ScreenEvents.h),
 *  so the shared judgements (Pokemon_SceneJudges.h) read PLZA frames too.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_H
#define PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_ButtonDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_OverworldPartySelectionDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The shared events from PLZA's detectors:
//  - DIALOG_BOX: a white dialogue box's bottom edge (FlatWhiteDialogDetector's
//    strip, without its advance arrow: the box shows while the text still
//    types), or a blue or teal box;
//  - CHOICE_BOX: the selection arrow among a dialogue's choices;
//  - INTERACT_PROMPT: an A prompt over the scene ("A Talk", "A Take Elevator");
//  - BATTLE_STARTED: the battle HUD's hints at the bottom centre ("ZL: Focus
//    on a Pokemon", "X A: Select Move"), read by their A icon. Story battles
//    show no Run prompt, so RunFromBattleDetector misses them;
//  - PLAYER_CAN_MOVE: the party bar at the bottom left, with no box;
//  - FADE_BLACK / FADE_WHITE: fade_of_screen().
//  Follow-up: the party bar is missed in some rooms (Hotel Z's hall,
//  za2_m3x), and the tutorial's "Move / Control Camera" hint has no detector
//  yet; both need frames measured before PLAYER_CAN_MOVE leans on them.
class PlzaScreenObserver : public Pokemon::ScreenEventObserver{
public:
    PlzaScreenObserver();

    virtual Pokemon::ScreenEvents observe(const VideoSnapshot& frame, WallClock now) override;
    virtual void make_overlays(VideoOverlaySet& items) const override;

private:
    BlueDialogDetector m_blue_dialog;
    TealDialogDetector m_teal_dialog;
    SelectionArrowDetector m_choice_arrow;
    ButtonDetector m_interact;
    ButtonDetector m_battle_hint;
    OverworldPartySelectionDetector m_party;
};

//  Registers the adapter's image tests (lab-B frames, 2026-11-08; they live
//  in CommandLineTests, PokemonLZA/AutoStoryScreenEvents/).
void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
