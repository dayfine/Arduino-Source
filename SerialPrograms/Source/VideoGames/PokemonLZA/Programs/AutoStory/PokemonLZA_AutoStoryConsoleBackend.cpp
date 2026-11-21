/*  PokemonLZA AutoStory - Console backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include "Common/Cpp/Color.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogChoiceBox.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_TutorialCardDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryConsoleBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


PlzaConsoleBackend::PlzaConsoleBackend(StoryEnv& env)
    : m_env(env)
    , m_home(env.env.console, COLOR_RED)
{}

void PlzaConsoleBackend::press(Button button, Milliseconds hold, Milliseconds release){
    pbf_press_button(m_env.context, button, hold, release);
}

void PlzaConsoleBackend::stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace){
    const double radians = degrees * std::numbers::pi / 180.0;
    const JoystickPosition position{std::sin(radians), std::cos(radians)};
    pbf_move_left_joystick(m_env.context, position, duration, release);
}

void PlzaConsoleBackend::wait(Milliseconds duration){
    m_env.context.wait_for_all_requests();
    m_env.context.wait_for(duration);
}

ScreenObservation PlzaConsoleBackend::observe(){
    m_env.context.wait_for_all_requests();
    VideoSnapshot frame = m_env.env.console.video().snapshot();
    ScreenObservation look;
    if (frame && m_home.detect(*frame.frame)){
        look.events.set(ScreenEvent::SWITCH_HOME_MENU, true);
    }else if (frame){
        look.events = m_observer.read(*frame.frame);
        if (look.events.has(ScreenEvent::CHOICE_BOX)){
            look.events.set(ScreenEvent::DIALOG_BOX, true);
        }
        if (m_autosave.detect(*frame.frame)){
            look.readings[std::string(AUTOSAVE_READING)] = 1;
        }
        if (m_controls_hint.detect(*frame.frame)){
            look.readings[std::string(CONTROLS_HINT_READING)] = 1;
        }
        if (m_advance_arrow.detect(*frame.frame)){
            look.readings[std::string(ADVANCE_ARROW_READING)] = 1;
        }
        if (tutorial_card_shows(*frame.frame)){
            look.readings[std::string(TUTORIAL_CARD_READING)] = 1;
        }
        if (look.events.has(ScreenEvent::CHOICE_BOX)){
            const std::optional<DialogChoiceRows> rows = read_dialog_choice_rows(*frame.frame);
            if (rows){
                look.readings[std::string(CHOICE_ROWS_READING)] = (int)rows->count;
                look.readings[std::string(CHOICE_SELECTED_READING)] = (int)rows->selected;
            }
        }
        if (same_screen(m_previous, frame)){
            look.events.set(ScreenEvent::SCREEN_STILL, true);
        }
    }
    look.when = current_time();
    m_previous = frame;
    look.frame = std::move(frame);
    return look;
}

void PlzaConsoleBackend::log(std::string_view line){
    m_env.env.console.log(std::string(line), COLOR_BLUE);
    m_env.env.console.overlay().add_log(std::string(line));
}

void PlzaConsoleBackend::log_detail(std::string_view line){
    m_env.env.console.log(std::string(line), COLOR_BLUE);
}

CancellableScope& PlzaConsoleBackend::scope(){
    return m_env.context;
}


namespace{

constexpr Milliseconds RELOAD_SETTLE = 1s;

std::optional<Pokemon::StoryFailure> reload_latest_save(StoryEnv& env){
    if (!reset_game_from_home(env.env, env.env.console, env.context, /*backup_save=*/false, RELOAD_SETTLE)){
        return Pokemon::StoryFailure{.code = "launch_failed", .detail = "the game did not reach the overworld after the reload"};
    }
    return std::nullopt;
}

} // namespace

Pokemon::StoryGame make_plza_story_game(StoryEnv& env, const MarkedCheckpoints& marked, AutosaveSightings& sightings){
    Pokemon::StoryGame game = plza_story_game(marked, sightings);
    game.reload = [&env, &sightings](PlzaBackend& backend, const Pokemon::StoryCheckpoint& failed) -> std::optional<std::string>{
        sightings.restart();
        if (const std::optional<Pokemon::StoryFailure> failure = reload_latest_save(env)){
            backend.log(std::format("[Save] {}: the reload failed: {}", failed.id, *failure));
            return std::nullopt;
        }
        return failed.id;
    };
    return game;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
