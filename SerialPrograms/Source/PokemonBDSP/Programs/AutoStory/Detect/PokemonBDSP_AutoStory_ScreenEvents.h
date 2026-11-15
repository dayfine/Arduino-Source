/*  BDSP AutoStory - Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's detectors as the shared screen events (Pokemon_ScreenEvents.h):
 *  the dialogue box, the overworld, a still screen, a fade, the expected
 *  map's location banner and a battle's start.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_ScreenEvents_H
#define PokemonAutomation_PokemonBDSP_AutoStory_ScreenEvents_H

#include <string>
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_LocationBannerWatcher.h"
#include "PokemonBDSP_AutoStory_OverworldDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  What BATTLE_STARTED is.
enum class BattleCue{
    //  A wild battle's menu. A black screen is a fade.
    WILD_MENU,
    //  Also a trainer's battle: its menu (Roark's read as STANDARD, r210d)
    //  or the black screen it opens with.
    TRAINER,
};

//  The shared events from BDSP's detectors. Not the choice box, the
//  interact prompt or a battle's end: no AutoStory step reads them yet.
class BdspScreenObserver : public Pokemon::ScreenEventObserver{
public:
    //  `banner`: the expected map's banner name; LOCATION_BANNER is only it
    //  ("" for none).
    BdspScreenObserver(BattleCue cue, std::string banner);

    virtual Pokemon::ScreenEvents observe(const VideoSnapshot& frame, WallClock now) override;
    virtual void make_overlays(VideoOverlaySet& items) const override;

private:
    BattleCue m_cue;
    std::string m_banner;
    ShortDialogDetector m_dialog;
    OverworldDetector m_overworld;
    BattleMenuDetector m_wild_menu;
    BattleMenuDetector m_trainer_menu;
    BlackScreenDetector m_black;
    LocationBannerReads m_reads;
    VideoSnapshot m_previous;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
