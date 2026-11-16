/*  PokemonLZA AutoStory - Story backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA on the console for the engine's backend
 *  (Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h).
 *
 *  Directions are the stick relative to the camera: north is straight on,
 *  east to the right. tap() turns that way (story_turn()); hold() walks.
 *  Dash is a toggle in this game and the walks assume it is off, so
 *  StoryPace::RUN walks too.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackend_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackend_H

#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryAdvanceArrowDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryAutosaveDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryControlsHintDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMainMissionBannerDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryPlatformArrivalDetector.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The console a run plays on, for the actions only a console can do.
struct StoryEnv{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
};

//  The engine's backend for PLZA's pose.
using PlzaBackend = Pokemon::StoryBackend<Pokemon::ZonePose>;

//  Turns the player `degrees` clockwise of straight on (negative: left),
//  as the story was recorded: L recenters the camera behind the player,
//  then the stick is tapped that way for 150 ms. The tap also takes a step.
void story_turn(PlzaBackend& backend, double degrees);


//  PLZA on the console: the controller and PlzaScreenObserver.
//  code-health: allow public_methods -- the engine's backend calls; nothing PLZA adds
class PlzaConsoleBackend : public PlzaBackend{
public:
    explicit PlzaConsoleBackend(StoryEnv& env);

    virtual Pokemon::StoryBackendKind kind() const override{
        return Pokemon::StoryBackendKind::CONSOLE;
    }
    virtual void press(Button button, Milliseconds hold, Milliseconds release) override;
    virtual void tap(Pokemon::Direction direction) override;
    virtual void hold(Pokemon::Direction direction, Milliseconds duration, Pokemon::StoryPace pace) override;
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace) override;
    virtual void wait(Milliseconds duration) override;

    //  Once the inputs are done: SWITCH_HOME_MENU alone on the Switch's
    //  HOME (PLZA's detectors may read its plain screen as the game); else
    //  PlzaScreenObserver's events on a frame (Pokemon::screen_observation()),
    //  SCREEN_STILL when it barely differs from the frame of the look before,
    //  and the readings PLATFORM_READING, AUTOSAVE_READING,
    //  CONTROLS_HINT_READING, ADVANCE_ARROW_READING and
    //  MISSION_BANNER_READING.
    virtual Pokemon::Observation<Pokemon::ZonePose> observe() override;

    //  To the console's log and the stream's overlay log: the checkpoint and
    //  the step under way, for whoever watches.
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
    VideoSnapshot m_previous;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
