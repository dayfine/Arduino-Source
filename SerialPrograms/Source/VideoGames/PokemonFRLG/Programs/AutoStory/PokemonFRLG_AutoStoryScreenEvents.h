/*  FRLG AutoStory - Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's detectors as the shared screen events (CommonTools/ScreenEvents/ScreenEvents.h),
 *  so the shared judgements (Pokemon_SceneJudges.h) read FRLG frames too.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryScreenEvents_H
#define PokemonAutomation_PokemonFRLG_AutoStoryScreenEvents_H

#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The shared events from FRLG's detectors:
//  - DIALOG_BOX: the overworld's white text box (WhiteDialogDetector);
//  - CHOICE_BOX: a Yes/No or answer box beside it (SelectionDialogDetector);
//  - BATTLE_STARTED: the battle menu or a battle's text box;
//  - FADE_BLACK / FADE_WHITE: fade_of_screen();
//  - QUIET_OVERWORLD where none of these shows, and on a dark cave's
//    overworld (dark_cave_screen()), whose black the battle text box
//    detector reads as a box.
//  Not reported yet, as FRLG has no detector for them here: the map-name
//  popup (LOCATION_BANNER), SCREEN_STILL, the interact prompt (FRLG has
//  none) and a battle's end.
//  The overworld at rest, as FrlgScreenObserver reports it: no box, fade
//  or battle. FRLG reads no box around the player, so such a screen counts
//  as the player in control and still; the shared settle
//  (Pokemon_Settle.h) ends on it once it holds for the rule's quiet. A
//  menu over the overworld reads the same, as it did when nothing was
//  reported for it.
inline const ScreenEvents QUIET_OVERWORLD{ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::PLAYER_STILL};

class FrlgScreenObserver : public ScreenEventObserver{
public:
    FrlgScreenObserver();

    virtual ScreenEvents observe(const VideoSnapshot& frame, WallClock now) override;
    virtual void make_overlays(VideoOverlaySet& items) const override;

private:
    WhiteDialogDetector m_dialog;
    SelectionDialogDetector m_choice;
    BattleMenuDetector m_battle_menu;
    BattleDialogDetector m_battle_dialog;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
