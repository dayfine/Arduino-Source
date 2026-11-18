/*  Home Screen Observer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_HomeScreenObserver_H
#define PokemonAutomation_NintendoSwitch_HomeScreenObserver_H

#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch_HomeMenuDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  A game's observer behind a look at the Switch itself: a frame showing
//  the HOME menu shows SWITCH_HOME_MENU and nothing else, as a game's
//  detectors may read HOME's plain screen as a quiet overworld (BDSP,
//  lab-B 2026-11-09). Every other frame is the game's to read.
class SwitchHomeObserver : public ScreenEventObserver{
public:
    SwitchHomeObserver(ConsoleHandle& console, ScreenEventObserver& game);

    virtual ScreenEvents observe(const VideoSnapshot& frame, WallClock now) override;
    virtual void make_overlays(VideoOverlaySet& items) const override;

private:
    ScreenEventObserver& m_game;
    HomeMenuDetector m_home;
};



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif
