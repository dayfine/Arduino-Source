/*  PokemonLZA AutoStory - Console backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Dash is a toggle in this game and the walks assume it is off, so
 *  StoryPace::RUN walks too.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryConsoleBackend_H
#define PokemonAutomation_PokemonLZA_AutoStoryConsoleBackend_H

#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_MainMenuDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryNewGameChoices.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryAdvanceArrowDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryAutosaveDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryControlsHintDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMainMissionBannerDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMissionCompleteDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryPlatformArrivalDetector.h"
#include "PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The console a run plays on.
struct StoryEnv{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    BattleAIOption& battle_ai;
    //  The language, look and name 01.00a picks (01.00b checks them).
    NewGameChoices new_game = {};
};

//  The console's MinimapArrowReader.
MinimapArrowReader minimap_arrow(StoryEnv& env);

//  The console's LockOnWatch, as upstream TrainerBattle's attempt_one_attack().
LockOnWatch move_symbols_lock_on(StoryEnv& env);


//  PLZA on the console.
//  code-health: allow public_methods -- the engine's backend calls; nothing PLZA adds
class PlzaConsoleBackend : public PlzaBackend{
public:
    explicit PlzaConsoleBackend(StoryEnv& env);

    virtual Pokemon::StoryBackendKind kind() const override{
        return Pokemon::StoryBackendKind::CONSOLE;
    }
    virtual void press(Button button, Milliseconds hold, Milliseconds release) override;
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace) override;
    virtual void wait(Milliseconds duration) override;

    //  Once the inputs are done. On the Switch's HOME, SWITCH_HOME_MENU alone:
    //  PLZA's detectors may read its plain screen as the game.
    virtual Pokemon::Observation<Pokemon::ZonePose> observe() override;
    //  Two minutes at most; fails as "battle_failed".
    virtual Pokemon::Outcome battle(const Pokemon::StoryActionArgs& args) override;

    //  Also to the stream's overlay log.
    virtual void log(std::string_view line) override;
    //  To the console's log only.
    virtual void log_detail(std::string_view line) override;
    virtual CancellableScope& scope() override;

private:
    StoryEnv& m_env;
    HomeMenuDetector m_home;
    PlzaScreenObserver m_observer;
    PlatformArrivalDetector m_platform;
    AutosaveDetector m_autosave;
    ControlsHintDetector m_controls_hint;
    AdvanceArrowDetector m_advance_arrow;
    MainMissionBannerDetector m_mission_banner;
    MissionCompleteDetector m_mission_complete;
    MainMenuDetector m_main_menu;
    SelectionArrowDetector m_save_button;
    VideoSnapshot m_previous;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
