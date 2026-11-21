/*  PokemonLZA AutoStory - Console backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Console-backed PlzaBackend, StoryEnv, and StoryGame factory.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryConsoleBackend_H
#define PokemonAutomation_PokemonLZA_AutoStoryConsoleBackend_H

#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_AutosaveDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_ControlsHintDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_StoryDialogDetector.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"
#include "PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Console handles shared across PLZA AutoStory.
struct StoryEnv{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
};

//  PlzaBackend implementation driving a live Switch console.
class PlzaConsoleBackend : public PlzaBackend{
public:
    explicit PlzaConsoleBackend(StoryEnv& env);

    virtual void press(Button button, Milliseconds hold, Milliseconds release) override;
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace) override;
    virtual void wait(Milliseconds duration) override;

    //  Waits for pending inputs, snapshots the video feed, and returns events and readings.
    //  On Switch HOME, reports SWITCH_HOME_MENU only.
    virtual ScreenObservation observe() override;

    //  Logs to both console log and stream overlay.
    virtual void log(std::string_view line) override;
    //  Logs to the console log only.
    virtual void log_detail(std::string_view line) override;
    virtual CancellableScope& scope() override;

private:
    StoryEnv& m_env;
    HomeMenuDetector m_home;
    PlzaScreenObserver m_observer;
    AutosaveDetector m_autosave;
    ControlsHintDetector m_controls_hint;
    AdvanceArrowDetector m_advance_arrow;
    VideoSnapshot m_previous;
};

//  Builds the console StoryGame with save-reload recovery.
//  `env` and `sightings` must outlive the returned game.
Pokemon::StoryGame make_plza_story_game(StoryEnv& env, const MarkedCheckpoints& marked, AutosaveSightings& sightings);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
