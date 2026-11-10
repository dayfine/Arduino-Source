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
#include "CommonFramework/Language.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"
#include "PokemonLZA_AutoStoryBadgeDetectors.h"
#include "PokemonLZA_AutoStoryObservation.h"
#include "PokemonLZA_AutoStoryOpeningDetectors.h"
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
    //  The new game's name: letters and digits; type_player_name() capitalizes
    //  the first letter only. The lab's player is Kosmos.
    std::string player_name = "Kosmos";
    //  The game's language, for the steps that read text.
    Language language = Language::English;
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
    //  Once the inputs are done: PlzaScreenObserver's events on a frame
    //  (plza_observation()), STILL when it barely differs from the frame
    //  of the look before, and the readings PLATFORM_READING,
    //  SKIP_PROMPT_READING, AUTOSAVE_READING, CONTROLS_HINT_READING and
    //  ADVANCE_ARROW_READING.
    virtual Pokemon::Observation<Pokemon::ZonePose> observe() override;
    //  fight_battle() with the run's battle AI; a failure is an Outcome.
    virtual Pokemon::Outcome battle(const Pokemon::StoryActionArgs& args) override;
    virtual void log(std::string_view line) override;
    virtual CancellableScope& scope() override;

private:
    StoryEnv& m_env;
    PlzaScreenObserver m_observer;
    PlatformArrivalDetector m_platform;
    SkipPromptDetector m_skip_prompt;
    AutosaveDetector m_autosave;
    ControlsHintDetector m_controls_hint;
    AdvanceArrowDetector m_advance_arrow;
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
