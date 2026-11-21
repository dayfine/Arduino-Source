/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  SceneTimeline names the screen a look shows (classify()) from the look
 *  and what earlier looks showed (record()); run_scene_machine() presses for
 *  it and SceneLog writes the lines.
 *
 */

#include <chrono>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/PrettyPrint.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenTimers.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStoryScreenPredicates.h"
#include "PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{


//  A sign of control held this long ends the machine (two looks or more):
//  the party bar also shows between a scene's lines.
constexpr Milliseconds CONTROL_HOLD = 1s;
constexpr Milliseconds POLL = 250ms;
constexpr Milliseconds PRESS_HOLD = 160ms;
constexpr Milliseconds PRESS_RELEASE = 240ms;


enum class SceneScreen{
    FADE,           //  waits: a fade decides nothing
    BLACK_FADE_END, //  terminal, with end_on_black_fade
    CHOICE,         //  A on its top row
    TEXT,           //  A, or B (text_button())
    CARD,           //  a tutorial card: A
    CONTROL,        //  terminal: a sign of control held CONTROL_HOLD
    CONTROL_SHOWING,
    OTHER,          //  a cutscene, a banner, a screen no detector reads: A, or B
};

struct SceneScreenName{
    SceneScreen screen;
    std::string_view name;
    //  Its line goes to the overlay too, not only to the file log.
    bool on_overlay;
};

constexpr SceneScreenName SCREEN_NAMES[] = {
    {SceneScreen::FADE, "fade", true},
    {SceneScreen::BLACK_FADE_END, "black_fade", false},
    {SceneScreen::CHOICE, "choice_top_row", true},
    {SceneScreen::TEXT, "text", false},
    {SceneScreen::CARD, "card", false},
    {SceneScreen::CONTROL, "control", false},
    {SceneScreen::CONTROL_SHOWING, "control_showing", true},
    {SceneScreen::OTHER, "other", false},
};

const SceneScreenName& name_of(SceneScreen screen){
    return SCREEN_NAMES[static_cast<size_t>(screen)];
}

bool terminal(SceneScreen screen){
    return screen == SceneScreen::BLACK_FADE_END || screen == SceneScreen::CONTROL;
}

bool hud_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) || look.readings.contains(CONTROLS_HINT_READING);
}

bool scene_under_way(const ScreenObservation& look){
    if (TEXT_SHOWS.matches(look) || look.events.has(ScreenEvent::CHOICE_BOX) || look.readings.contains(TUTORIAL_CARD_READING)){
        return true;
    }
    if (hud_shows(look)){
        return false;
    }
    return FADE_SHOWS.matches(look) || !look.events.has(ScreenEvent::SCREEN_STILL);
}

constexpr std::string_view EXPECTED_END = "the controls hint or the party bar";


//  The time-dependent part of the machine; a look's time is its `when`.
//  classify() reads the state; record() then adds the look to it.
class SceneTimeline{
public:
    explicit SceneTimeline(SceneMachineConfig config)
        : m_config(config)
    {}

    SceneScreen classify(const ScreenObservation& look) const;
    void record(const ScreenObservation& look);

    //  B, not A: on a look with a sign of control and once one has shown
    //  in the scene, A starts a talk.
    Button text_button(const ScreenObservation& look) const;
    Button other_button() const;

    size_t black_fades() const{
        return m_black_fades;
    }

private:
    SceneMachineConfig m_config;
    bool m_scene_seen = false;
    //  A sign of control after the scene started.
    bool m_overworld_seen = false;
    Pokemon::HoldTimer m_control_hold{CONTROL_HOLD};
    size_t m_black_fades = 0;
};

SceneScreen SceneTimeline::classify(const ScreenObservation& look) const{
    if (FADE_SHOWS.matches(look)){
        return m_config.end_on_black_fade && look.events.has(ScreenEvent::FADE_BLACK) ? SceneScreen::BLACK_FADE_END : SceneScreen::FADE;
    }
    if (look.events.has(ScreenEvent::CHOICE_BOX)){
        return SceneScreen::CHOICE;
    }
    if (TEXT_SHOWS.matches(look)){
        return SceneScreen::TEXT;
    }
    if (CARD_SHOWS.matches(look)){
        return SceneScreen::CARD;
    }
    if (CONTROL_SHOWS.matches(look)){
        return m_control_hold.held(look.when) ? SceneScreen::CONTROL : SceneScreen::CONTROL_SHOWING;
    }
    return SceneScreen::OTHER;
}

Button SceneTimeline::text_button(const ScreenObservation& look) const{
    return hud_shows(look) || m_overworld_seen ? BUTTON_B : BUTTON_A;
}

Button SceneTimeline::other_button() const{
    return m_overworld_seen ? BUTTON_B : BUTTON_A;
}

void SceneTimeline::record(const ScreenObservation& look){
    m_black_fades += look.events.has(ScreenEvent::FADE_BLACK) ? 1 : 0;
    m_overworld_seen = m_overworld_seen || (m_scene_seen && CONTROL_SHOWS.matches(look));
    m_scene_seen = m_scene_seen || scene_under_way(look);
    m_control_hold.update(CONTROL_SHOWS.matches(look), look.when);
}


std::string describe_look(const ScreenObservation& look){
    const std::pair<ScreenEvent, std::string_view> events[] = {
        {ScreenEvent::FADE_BLACK, "black fade"}, {ScreenEvent::FADE_WHITE, "white fade"},
        {ScreenEvent::DIALOG_BOX, "text box"}, {ScreenEvent::CHOICE_BOX, "choice box"},
        {ScreenEvent::PLAYER_CAN_MOVE, "party bar"}, {ScreenEvent::SCREEN_STILL, "still"},
    };
    const std::pair<std::string_view, std::string_view> readings[] = {
        {CONTROLS_HINT_READING, "controls hint"}, {ADVANCE_ARROW_READING, "advance arrow"},
        {TUTORIAL_CARD_READING, "tutorial card"},
    };
    std::string text;
    const auto add = [&text](std::string_view name){
        text += std::format("{}{}", text.empty() ? "" : ", ", name);
    };
    for (const auto& [event, name] : events){
        if (look.events.has(event)){
            add(name);
        }
    }
    for (const auto& [reading, name] : readings){
        if (look.readings.contains(reading)){
            add(name);
        }
    }
    return text.empty() ? "nothing" : text;
}


//  A line per change of screen or readings ("[PLZA label] screen 3: other at 12.5s: still").
class SceneLog{
public:
    SceneLog(PlzaBackend& backend, std::string_view label, WallClock start)
        : m_backend(backend)
        , m_label(label)
        , m_start(start)
    {}

    void note(std::string_view text){
        m_backend.log(std::format("[PLZA {}] {}", m_label, text));
    }
    void look(SceneScreen screen, std::string_view readings, WallClock when){
        const SceneScreenName& name = name_of(screen);
        std::string key = std::format("{}: {}", name.name, readings);
        if (key == m_last_key){
            return;
        }
        if (screen != m_screen){
            m_screen = screen;
            m_screen_number++;
        }
        m_last_key = std::move(key);
        const double seconds = std::chrono::duration<double>(when - m_start).count();
        const std::string line = std::format(
            "[PLZA {}] screen {}: {} at {}s: {}", m_label, m_screen_number, name.name, tostr_fixed(seconds, 1), readings
        );
        if (name.on_overlay){
            m_backend.log(line);
        }else{
            m_backend.log_detail(line);
        }
    }

private:
    PlzaBackend& m_backend;
    std::string m_label;
    WallClock m_start;
    std::optional<SceneScreen> m_screen;
    size_t m_screen_number = 0;
    std::string m_last_key;
};


//  A choice: Up until the top row is highlighted, then A.
Button choice_button(const ScreenObservation& look, SceneLog& log){
    const auto rows = look.readings.find(CHOICE_ROWS_READING);
    const auto selected = look.readings.find(CHOICE_SELECTED_READING);
    if (rows == look.readings.end() || selected == look.readings.end()){
        log.note("choice: rows not read: A");
        return BUTTON_A;
    }
    if (selected->second > 0){
        log.note(std::format("choice: {} rows, row {} highlighted: Up to row 0", rows->second, selected->second));
        return BUTTON_UP;
    }
    log.note(std::format("choice: {} rows, row 0 highlighted: A", rows->second));
    return BUTTON_A;
}

//  The button `screen` takes, none for a wait.
std::optional<Button> button_for(SceneScreen screen, const ScreenObservation& look, const SceneTimeline& timeline, SceneLog& log){
    switch (screen){
    case SceneScreen::CHOICE:
        return choice_button(look, log);
    case SceneScreen::TEXT:
        return timeline.text_button(look);
    case SceneScreen::CARD:
        return BUTTON_A;
    case SceneScreen::OTHER:
        return timeline.other_button();
    default:
        return std::nullopt;
    }
}

} // namespace


std::string describe_scene_look(const ScreenObservation& look){
    return describe_look(look);
}

std::optional<Pokemon::StoryFailure> run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout
){
    backend.log(std::format("[PLZA {}] start, expecting {}.", label, EXPECTED_END));
    ScreenObservation look = backend.observe();
    const WallClock start = look.when;
    SceneTimeline timeline(config);
    SceneLog log(backend, label, start);
    //  Each look waits or presses at least POLL; this bounds a script whose time stands still.
    const size_t max_looks = (size_t)(timeout / POLL) + 1;
    for (size_t looks = 1; ; looks++){
        backend.scope().throw_if_cancelled();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return Pokemon::game_not_running_failure();
        }
        const SceneScreen screen = timeline.classify(look);
        const std::optional<Button> button = button_for(screen, look, timeline, log);
        timeline.record(look);
        std::string readings = describe_look(look);
        if (look.events.has(ScreenEvent::FADE_BLACK)){
            readings = std::format("{}, black fade {}", readings, timeline.black_fades());
        }
        log.look(screen, readings, look.when);
        if (button){
            backend.press(*button, PRESS_HOLD, PRESS_RELEASE);
        }else if (!terminal(screen)){
            backend.wait(POLL);
        }
        if (terminal(screen)){
            backend.log(std::format("[PLZA {}] done on {}.", label, name_of(screen).name));
            return std::nullopt;
        }
        if (look.when - start >= timeout || looks >= max_looks){
            return Pokemon::StoryFailure{.code = "scene_timeout", .detail = std::format(
                "{}: Did not finish within {} s (expecting {}).", label, std::chrono::duration_cast<Seconds>(timeout).count(), EXPECTED_END
            )};
        }
        look = backend.observe();
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
