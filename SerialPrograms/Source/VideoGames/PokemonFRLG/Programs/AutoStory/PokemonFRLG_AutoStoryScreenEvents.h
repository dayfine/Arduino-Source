/*  FRLG AutoStory - Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's detectors as the shared screen events (Pokemon_ScreenEvents.h),
 *  so the shared judgements (Pokemon_SceneJudges.h) read FRLG frames too.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryScreenEvents_H
#define PokemonAutomation_PokemonFRLG_AutoStoryScreenEvents_H

#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The shared events from FRLG's detectors:
//  - DIALOG_BOX: the overworld's white text box (WhiteDialogDetector);
//  - CHOICE_BOX: a Yes/No or answer box beside it (SelectionDialogDetector);
//  - BATTLE_STARTED: the battle menu or a battle's text box;
//  - FADE_BLACK / FADE_WHITE: fade_of_screen().
//  Not reported yet, as FRLG has no detector for them here: the map-name
//  popup (LOCATION_BANNER), PLAYER_CAN_MOVE, SCREEN_STILL, the interact
//  prompt (FRLG has none) and a battle's end.
class FrlgScreenObserver : public Pokemon::ScreenEventObserver{
public:
    FrlgScreenObserver();

    virtual Pokemon::ScreenEvents observe(const VideoSnapshot& frame, WallClock now) override;
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
