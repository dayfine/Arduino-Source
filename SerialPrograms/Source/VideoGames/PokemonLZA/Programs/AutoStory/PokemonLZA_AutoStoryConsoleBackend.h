/*  PokemonLZA AutoStory - Console backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  StoryPace::RUN holds B with the stick for the whole leg (a press
 *  inside a leg toggles dash) and plays a walk time at run pace.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryConsoleBackend_H
#define PokemonAutomation_PokemonLZA_AutoStoryConsoleBackend_H

#include <optional>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "VideoGames/PokemonLZA/Inference/Map/PokemonLZA_MapDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_MainMenuDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryNewGameChoices.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryAdvanceArrowDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryAutosaveDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryControlsHintDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMainMissionBannerDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMissionCompleteDetector.h"

#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryHeading.h"
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
    //  Set while a checkpoint before the one where B first runs plays: run
    //  legs walk at their walk times.
    bool walk_only = false;
};

//  The console's MinimapArrowReader.
MinimapArrowReader minimap_arrow(StoryEnv& env);

//  The console's LockOnWatch, as upstream TrainerBattle's attempt_one_attack().
LockOnWatch move_symbols_lock_on(StoryEnv& env);


//  PLZA on the console.
class PlzaConsoleBackend : public PlzaBackend{
public:
    explicit PlzaConsoleBackend(StoryEnv& env);

    virtual void press(Button button, Milliseconds hold, Milliseconds release) override;
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace) override;
    virtual void wait(Milliseconds duration) override;

    //  Once the inputs are done. On the Switch's HOME, SWITCH_HOME_MENU alone:
    //  PLZA's detectors may read its plain screen as the game.
    virtual ScreenObservation observe() override;

    //  Also to the stream's overlay log.
    virtual void log(std::string_view line) override;
    //  To the console's log only.
    virtual void log_detail(std::string_view line) override;
    virtual CancellableScope& scope() override;

private:
    //  Waits for the queued inputs, or during a run only until just before
    //  it ends (m_run_end).
    void wait_for_inputs();
    //  The main menu, its Save button and the map.
    void read_menus(const ImageViewRGB32& frame, ScreenObservation& look);

    StoryEnv& m_env;
    HomeMenuDetector m_home;
    PlzaScreenObserver m_observer;

    AutosaveDetector m_autosave;
    ControlsHintDetector m_controls_hint;
    AdvanceArrowDetector m_advance_arrow;
    MainMissionBannerDetector m_mission_banner;
    MissionCompleteDetector m_mission_complete;
    MainMenuDetector m_main_menu;
    MapDetector m_map;
    SelectionArrowDetector m_save_button;
    VideoSnapshot m_previous;
    //  When the queued run (B held) ends. A look during a run waits only until
    //  just before then, so the next slice queues behind it and B never lets go:
    //  a new press of B inside a leg switches dash off.
    std::optional<WallClock> m_run_end;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
