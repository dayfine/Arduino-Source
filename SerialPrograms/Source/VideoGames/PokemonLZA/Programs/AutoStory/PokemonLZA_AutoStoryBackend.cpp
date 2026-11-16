/*  PokemonLZA AutoStory - Story backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include "Common/Cpp/Color.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenObservation.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryBattles.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDialogChoiceBox.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::Direction;


PlzaConsoleBackend::PlzaConsoleBackend(StoryEnv& env)
    : m_env(env)
    , m_home(env.env.console, COLOR_RED)
{}

void PlzaConsoleBackend::press(Button button, Milliseconds hold, Milliseconds release){
    pbf_press_button(m_env.context, button, hold, release);
}

void PlzaConsoleBackend::tap(Direction direction){
    story_turn(*this, Pokemon::direction_degrees(direction));
}

void PlzaConsoleBackend::hold(Direction direction, Milliseconds duration, Pokemon::StoryPace pace){
    stick(Pokemon::direction_degrees(direction), duration, /*release=*/0ms, pace);
}

void PlzaConsoleBackend::stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace){
    const double radians = degrees * std::numbers::pi / 180.0;
    pbf_move_left_joystick(m_env.context, {std::sin(radians), std::cos(radians)}, duration, release);
}

void PlzaConsoleBackend::wait(Milliseconds duration){
    m_env.context.wait_for(duration);
}

Pokemon::Observation<Pokemon::ZonePose> PlzaConsoleBackend::observe(){
    m_env.context.wait_for_all_requests();
    VideoSnapshot frame = m_env.env.console.video().snapshot();
    Pokemon::Observation<Pokemon::ZonePose> look;
    if (frame && m_home.detect(*frame.frame)){
        look.events.set(ScreenEvent::SWITCH_HOME_MENU, true);
    }else if (frame){
        static_cast<ScreenObservation&>(look) = Pokemon::screen_observation(m_observer.read(*frame.frame));
        if (m_platform.detect(*frame.frame)){
            look.readings[std::string(PLATFORM_READING)] = 1;
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
        if (m_mission_complete.detect(*frame.frame)){
            look.readings[std::string(MISSION_COMPLETE_READING)] = 1;
        }else if (m_mission_banner.detect(*frame.frame)){
            look.readings[std::string(MISSION_BANNER_READING)] = 1;
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

Pokemon::Outcome PlzaConsoleBackend::battle(){
    try{
        fight_story_battle(m_env.env, m_env.env.console, m_env.context, m_env.battle_ai, /*timeout=*/120s);
    }catch (OperationFailedException& e){
        return Pokemon::outcome_failed("battle_failed", e.message());
    }
    return Pokemon::outcome_ok();
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


void story_turn(PlzaBackend& backend, double degrees){
    //  150 ms is the tap the story was recorded with.
    //  Short releases: the legs run back to back, as when played by hand.
    backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
    backend.stick(degrees, /*duration=*/150ms, /*release=*/100ms, Pokemon::StoryPace::WALK);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
