/*  PokemonLZA AutoStory - Story backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA on the console for the engine's backend
 *  (Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h), and the inputs a game
 *  with no tiles builds from it.
 *
 *  Directions are the stick relative to the camera: north is straight on,
 *  east to the right. tap() turns that way (story_turn()); hold() walks.
 *  Dash is a toggle in this game and the walks assume it is off, so
 *  StoryPace::RUN walks too.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackend_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackend_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryAdvanceArrowDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryAutosaveDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryControlsHintDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMainMissionBannerDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMissionCompleteDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryPlatformArrivalDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStorySkipPromptDetector.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The starters Mission 1 offers. The player picks one by walking up to it
//  and talking to it.
enum class StarterChoice{
    CHIKORITA,
    TEPIG,
    TOTODILE,
};

//  The console a run plays on, for the actions only a console can do
//  (menus, OCR, the battle AI).
struct StoryEnv{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    //  The language the game is set to, for the steps that read text (the
    //  fast travel menu's place names).
    Language language = Language::None;
    BattleAIOption& battle_ai;
};


//  The engine's backend for PLZA's pose.
using PlzaBackend = Pokemon::StoryBackend<Pokemon::ZonePose>;
//  A reader for PLZA's pose (StoryGame::readers).
using PlzaReader = Pokemon::StoryReader<Pokemon::ZonePose>;

//  Turns the player `degrees` clockwise of straight on (negative: left),
//  as the story was recorded: L recenters the camera behind the player,
//  then the stick is tapped that way for 150 ms. The tap also takes a step.
void story_turn(PlzaBackend& backend, double degrees);

//  The STORY_HEADING reading for the console: the minimap's arrow
//  (get_facing_direction()), clockwise from north in [0, 360). It waits up
//  to 40 s for the minimap and throws without it, so its StoryGameReader
//  in make_plza_story_game() is ON_REQUEST and console_only: no
//  postcondition check runs it.
PlzaReader heading_reader(StoryEnv& env);


//  Where the game autosaved, for the log: each appearance of the autosave
//  badge once, as "[Autosave] 02.04/3 check_in", the event the engine last
//  logged as started. The badge shows 1-3 s; one looked away from (a missed
//  look) less than AUTOSAVE_GAP is the same appearance.
class AutosaveSpots{
public:
    //  Takes in a log line; "[Event] 02.04/3 check_in: start" (the engine's
    //  event_log_line()) names the event.
    void on_log(std::string_view line);

    //  A look at `when`: the log line when it shows a new appearance.
    std::optional<std::string> on_look(bool badge, WallClock when);

private:
    std::string m_event = "(before the first event)";
    std::optional<WallClock> m_last_badge;
};


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
    //  HOME (PLZA's detectors may read its plain screen as the overworld);
    //  else PlzaScreenObserver's events on a frame
    //  (Pokemon::screen_observation()), STILL when it barely differs from the frame
    //  of the look before, and the readings PLATFORM_READING,
    //  SKIP_PROMPT_READING, AUTOSAVE_READING, CONTROLS_HINT_READING,
    //  ADVANCE_ARROW_READING, MISSION_BANNER_READING and
    //  MISSION_COMPLETE_READING (which wins over the banner: the card has
    //  the banner's bar), and TUTORIAL_CARD_READING. Logs each appearance
    //  of the autosave badge (AutosaveSpots).
    virtual Pokemon::Observation<Pokemon::ZonePose> observe() override;
    //  fight_battle() with the run's battle AI; a failure is an Outcome.
    virtual Pokemon::Outcome battle(const Pokemon::StoryActionArgs& args) override;
    virtual void log(std::string_view line) override;
    virtual CancellableScope& scope() override;

private:
    StoryEnv& m_env;
    HomeMenuDetector m_home;
    PlzaScreenObserver m_observer;
    PlatformArrivalDetector m_platform;
    SkipPromptDetector m_skip_prompt;
    AutosaveDetector m_autosave;
    AutosaveSpots m_autosave_spots;
    ControlsHintDetector m_controls_hint;
    AdvanceArrowDetector m_advance_arrow;
    MainMissionBannerDetector m_mission_banner;
    MissionCompleteDetector m_mission_complete;
    VideoSnapshot m_previous;
    //  Every look's events, and a frame each time they change (and every
    //  CAPTURE_EVERY), go to the log and a capture folder: the record of
    //  what the detectors saw, to find the screens they miss.
    void capture(const Pokemon::Observation<Pokemon::ZonePose>& look);
    std::string m_capture_folder;
    std::optional<std::string> m_captured_text;
    WallClock m_captured_at;
    size_t m_captures = 0;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
