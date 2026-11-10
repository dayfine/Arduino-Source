/*  PokemonLZA AutoStory - Story backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <numbers>
#include <string>
#include <format>
#include "CommonFramework/GlobalAutoPaths.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "PokemonLZA_AutoStoryActions.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

//  Below this pixel RMSD two looks are the same screen.
constexpr double STILL_RMSD = 2.0;

//  `direction` as degrees clockwise of straight on.
double direction_degrees(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return 0;
    case Direction::EAST:
        return 90;
    case Direction::SOUTH:
        return 180;
    case Direction::WEST:
        return -90;
    }
    return 0;
}

bool same_screen(const VideoSnapshot& a, const VideoSnapshot& b){
    if (!a || !b || a.frame->width() != b.frame->width() || a.frame->height() != b.frame->height()){
        return false;
    }
    return ImageMatch::pixel_RMSD(*a.frame, *b.frame) < STILL_RMSD;
}

} // namespace


PlzaConsoleBackend::PlzaConsoleBackend(StoryEnv& env)
    : m_env(env)
{}

void PlzaConsoleBackend::press(Button button, Milliseconds hold, Milliseconds release){
    pbf_press_button(m_env.context, button, hold, release);
}

void PlzaConsoleBackend::tap(Direction direction){
    story_turn(*this, direction_degrees(direction));
}

void PlzaConsoleBackend::hold(Direction direction, Milliseconds duration, Pokemon::StoryPace pace){
    stick(direction_degrees(direction), duration, 0ms, pace);
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
    if (frame){
        static_cast<ScreenObservation&>(look) = plza_observation(m_observer.read(*frame.frame));
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
            look.events.add(PokemonAutomation::ScreenEvent::STILL);
        }
    }
    look.when = current_time();
    m_previous = frame;
    look.frame = std::move(frame);
    capture(look);
    return look;
}

namespace{

std::string events_text(const PokemonAutomation::ScreenEvents& events){
    using EngineEvent = PokemonAutomation::ScreenEvent;
    const std::pair<EngineEvent, std::string_view> names[] = {
        {EngineEvent::FADE_BLACK, "fade_black"},
        {EngineEvent::FADE_WHITE, "fade_white"},
        {EngineEvent::TEXT_BOX, "text_box"},
        {EngineEvent::BATTLE, "battle"},
        {EngineEvent::CAN_MOVE, "can_move"},
        {EngineEvent::STILL, "still"},
        {EngineEvent::CHOICE_BOX, "choice_box"},
        {EngineEvent::INTERACT_PROMPT, "interact_prompt"},
    };
    std::string text;
    for (const auto& [event, name] : names){
        if (events.has(event)){
            text += text.empty() ? "" : "+";
            text += name;
        }
    }
    return text.empty() ? std::string("nothing") : text;
}

//  The look's events and readings: "fade_black+still",
//  "nothing+skip_prompt", "can_move+platform+autosave+controls_hint".
std::string look_text(const ScreenObservation& look){
    std::string text = events_text(look.events);
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
    }catch (StoryActionNotImplemented&){
        throw;
    }catch (OperationFailedException& e){
        return Pokemon::outcome_failed("battle_failed", e.message());
    }
    return Pokemon::outcome_ok();
}

void PlzaConsoleBackend::log(std::string_view line){
    m_env.env.console.log(std::string(line), COLOR_BLUE);
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
