/*  PokemonLZA AutoStory - Story backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <format>
#include "CommonFramework/GlobalAutoPaths.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Color.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenObservation.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryActionNotImplemented.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryActions.h"
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
    stick(Pokemon::direction_degrees(direction), duration, 0ms, pace);
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
        look.events.set(PokemonAutomation::ScreenEvent::SWITCH_HOME_MENU, true);
    }else if (frame){
        static_cast<ScreenObservation&>(look) = Pokemon::screen_observation(m_observer.read(*frame.frame));
        if (m_platform.detect(*frame.frame)){
            look.readings[std::string(PLATFORM_READING)] = 1;
        }
        if (m_skip_prompt.detect(*frame.frame)){
            look.readings[std::string(SKIP_PROMPT_READING)] = 1;
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
        if (same_screen(m_previous, frame)){
            look.events.set(PokemonAutomation::ScreenEvent::SCREEN_STILL, true);
        }
    }
    look.when = current_time();
    const std::optional<std::string> autosave_line =
        m_autosave_spots.on_look(look.readings.contains(AUTOSAVE_READING), look.when);
    if (autosave_line){
        log(*autosave_line);
    }
    m_previous = frame;
    look.frame = std::move(frame);
    capture(look);
    return look;
}

namespace{

//  The look's events and readings, for a capture's file name:
//  "black_fade+screen_still", "nothing+skip_prompt",
//  "player_can_move+platform+autosave+controls_hint".
std::string look_text(const ScreenObservation& look){
    std::string text = describe(look.events, "+");
    std::replace(text.begin(), text.end(), ' ', '_');
    if (look.readings.contains(PLATFORM_READING)){
        text += "+platform";
    }
    if (look.readings.contains(SKIP_PROMPT_READING)){
        text += "+skip_prompt";
    }
    if (look.readings.contains(AUTOSAVE_READING)){
        text += "+autosave";
    }
    if (look.readings.contains(CONTROLS_HINT_READING)){
        text += "+controls_hint";
    }
    if (look.readings.contains(ADVANCE_ARROW_READING)){
        text += "+advance_arrow";
    }
    if (look.readings.contains(MISSION_BANNER_READING)){
        text += "+mission_banner";
    }
    if (look.readings.contains(MISSION_COMPLETE_READING)){
        text += "+mission_complete";
    }
    return text;
}

constexpr Milliseconds CAPTURE_EVERY(2000);

} // namespace

void PlzaConsoleBackend::capture(const Pokemon::Observation<Pokemon::ZonePose>& look){
    if (!look.frame || !look.frame->frame){
        return;
    }
    const std::string text = look_text(look);
    const bool changed = m_captured_text != text;
    if (!changed && look.when - m_captured_at < CAPTURE_EVERY){
        return;
    }
    if (m_capture_folder.empty()){
        m_capture_folder = std::format("{}PLZA-capture/{}/", SCREENSHOTS_PATH(), now_to_filestring());
        Filesystem::create_directories(Filesystem::Path(m_capture_folder));
    }
    m_captures++;
    const std::string path = std::format("{}{:04}-{}.png", m_capture_folder, m_captures, text);
    look.frame->frame->save(path);
    m_env.env.console.log(std::format("[PLZA capture] {:04} {}{}, platform {}", m_captures, text,
        changed ? " (changed)" : "", to_string(m_platform.last_distances())));
    m_captured_text = text;
    m_captured_at = look.when;
}

Pokemon::Outcome PlzaConsoleBackend::battle(const Pokemon::StoryActionArgs&){
    try{
        fight_battle(m_env.env, m_env.env.console, m_env.context, m_env.battle_ai);
    }catch (Pokemon::StoryActionNotImplemented&){
        throw;
    }catch (OperationFailedException& e){
        return Pokemon::outcome_failed("battle_failed", e.message());
    }
    return Pokemon::outcome_ok();
}

namespace{

//  A look without the badge shorter than this is the same appearance.
constexpr Milliseconds AUTOSAVE_GAP(4000);

} // namespace

void AutosaveSpots::on_log(std::string_view line){
    constexpr std::string_view EVENT = "[Event] ";
    constexpr std::string_view STARTED = ": start";
    if (!line.starts_with(EVENT) || !line.ends_with(STARTED)){
        return;
    }
    m_event = std::string(line.substr(EVENT.size(), line.size() - EVENT.size() - STARTED.size()));
}

std::optional<std::string> AutosaveSpots::on_look(bool badge, WallClock when){
    if (!badge){
        return std::nullopt;
    }
    const bool same = m_last_badge && when - *m_last_badge < AUTOSAVE_GAP;
    m_last_badge = when;
    if (same){
        return std::nullopt;
    }
    return std::format("[Autosave] {}", m_event);
}

void PlzaConsoleBackend::log(std::string_view line){
    m_autosave_spots.on_log(line);
    m_env.env.console.log(std::string(line), COLOR_BLUE);
    //  The checkpoint, the step under way and the autosaves, for whoever
    //  watches the stream: the engine's lines all come through here.
    m_env.env.console.overlay().add_log(std::string(line));
}

CancellableScope& PlzaConsoleBackend::scope(){
    return m_env.context;
}

void story_turn(PlzaBackend& backend, double degrees){
    //  150 ms is the tap the story was recorded with (console B, 2026-11-08).
    //  Short releases: the legs run back to back, as when played by hand.
    backend.press(BUTTON_L, 100ms, 150ms);
    backend.stick(degrees, 150ms, 100ms, Pokemon::StoryPace::WALK);
}

PlzaReader heading_reader(StoryEnv& env){
    return [&env](PlzaBackend&, Pokemon::Observation<Pokemon::ZonePose>& observation){
        const double heading = get_facing_direction(env.env.console, env.context);
        observation.readings[std::string(Pokemon::STORY_HEADING)] = (int)std::lround(heading) % 360;
    };
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
