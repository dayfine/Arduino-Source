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
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_PartyBarDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryBattleMarkersDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryBattlePanelDetector.h"


namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The shared events from PLZA's detectors. No DIALOG_BOX: text is the
//  backend's advance arrow reading, and PLAYER_CAN_MOVE (the party bar)
//  also shows under a talk's text. BATTLE_STARTED is the HUD's hints (only
//  while locked on) or the player's panel with the battle markers (the panel
//  alone shows all through a wild zone); story battles show no Run prompt.

class PlzaScreenObserver : public ScreenEventObserver{
public:
    PlzaScreenObserver();

    virtual ScreenEvents observe(const VideoSnapshot& frame, WallClock now) override;

    //  observe() for a frame that is not a snapshot.
    ScreenEvents read(const ImageViewRGB32& screen);

    virtual void make_overlays(VideoOverlaySet& items) const override;

private:

    SelectionArrowDetector m_choice_arrow;
    ButtonDetector m_interact;
    ButtonDetector m_battle_hint;
    BattlePanelDetector m_battle_panel;
    BattleMarkersDetector m_battle_markers;
    PartyBarDetector m_party;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
