/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  SceneTimeline names the screen a look shows (classify()) from the look
 *  and what earlier looks showed (record()); SceneRunner presses for it and
 *  SceneLog writes the lines.
 *
 */

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/PrettyPrint.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenTimers.h"
#include "Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "PokemonLZA_AutoStoryBattleOnScreen.h"
#include "PokemonLZA_AutoStoryMenuReadings.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStoryScreenPredicates.h"
#include "PokemonLZA_AutoStorySignalProbe.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{


//  A sign of control held this long ends the machine (two looks or more):
//  the party bar also shows between a scene's lines.
constexpr Milliseconds CONTROL_HOLD = 1s;
//  With stop_at_battle or stop_at_choice, the party bar or an A prompt this
//  long with nothing busy is the scene over with no battle or choice.
constexpr Milliseconds CONTROL_INSTEAD_HOLD = 5s;
constexpr Milliseconds POLL = 250ms;
//  The battle HUD shows ~1 s after the fade when the camera faces the
//  opponent; a still screen this long after is a battle off camera (03.02b).
constexpr Milliseconds LOCK_ON_AFTER = 3s;
constexpr size_t LOCK_ON_ATTEMPTS = 5;
//  TrainerBattle's attempt_one_attack() looks 1 s, the catches' sweep 1.5 s.
constexpr Milliseconds LOCK_ON_HOLD = 2s;
constexpr Milliseconds PRESS_HOLD = 160ms;
constexpr Milliseconds PRESS_RELEASE = 240ms;
//  The log sums up repeated looks this often.
constexpr Milliseconds SUMMARY_EVERY = 30s;
//  text_a_to_choice: one A per text page; the same page this long after
//  its A takes another (a dropped press).
constexpr Milliseconds TEXT_A_AGAIN = 2s;


enum class SceneScreen{
    BATTLE,         //  terminal, with stop_at_battle or end_at_battle
    LOCKED_ON,      //  terminal: the move symbols showed under ZL
    FADE,           //  waits: a fade decides nothing
    BLACK_FADE_END, //  terminal, with end_on_black_fade
    CHOICE_END,     //  terminal, with stop_at_choice
    CHOICE,         //  A on its top row
    TEXT,           //  A, or B (text_button())
    MAP,            //  B
    LOCK_ON,        //  ZL held while config.lock_on watches
    CARD,           //  a tutorial card or "mission complete": A
    NEVER_STARTED,  //  failing: the HUD for start_window and no scene
    AWAITING_SCENE,
    ENDED_IN_CONTROL,   //  failing: stop_at_battle/choice, but control held CONTROL_INSTEAD_HOLD
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
    {SceneScreen::BATTLE, "battle", false},
    {SceneScreen::LOCKED_ON, "locked_on", false},
    {SceneScreen::FADE, "fade", true},
    {SceneScreen::BLACK_FADE_END, "black_fade", false},
    {SceneScreen::CHOICE_END, "choice", false},
    {SceneScreen::CHOICE, "choice_top_row", true},
    {SceneScreen::TEXT, "text", false},
    {SceneScreen::MAP, "map", true},
    {SceneScreen::LOCK_ON, "lock_on", false},
    {SceneScreen::CARD, "card", false},
    {SceneScreen::NEVER_STARTED, "never_started", false},
    {SceneScreen::AWAITING_SCENE, "awaiting_scene", false},
    {SceneScreen::ENDED_IN_CONTROL, "ended_in_control", false},
    {SceneScreen::CONTROL, "control", false},
    {SceneScreen::CONTROL_SHOWING, "control_showing", true},
    {SceneScreen::OTHER, "other", false},
};

const SceneScreenName& name_of(SceneScreen screen){
    return SCREEN_NAMES[static_cast<size_t>(screen)];
}

bool terminal(SceneScreen screen){
    switch (screen){
    case SceneScreen::BATTLE:
    case SceneScreen::LOCKED_ON:
    case SceneScreen::BLACK_FADE_END:
    case SceneScreen::CHOICE_END:
    case SceneScreen::NEVER_STARTED:
    case SceneScreen::ENDED_IN_CONTROL:
    case SceneScreen::CONTROL:
        return true;
    default:
        return false;
    }
}


bool hud_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) || look.readings.contains(CONTROLS_HINT_READING) ||
        look.events.has(ScreenEvent::INTERACT_PROMPT);
}

//  Not when stopping at a choice or a battle: the talk shows them between lines.
bool party_bar_is_control(const SceneMachineConfig& config){
    return !config.stop_at_choice && !config.stop_at_battle;
}

bool ends_machine(const SceneMachineConfig& config, const ScreenObservation& look){
    return party_bar_is_control(config) ? CONTROL_SHOWS.matches(look) : look.readings.contains(CONTROLS_HINT_READING) && !look.events.has(ScreenEvent::DIALOG_BOX);
}

//  "the battle HUD, the choice box, the controls hint, the party bar or the A prompt".
std::string expected_end(const SceneMachineConfig& config){
    std::string ends;
    if (config.stop_at_battle){
        ends += "the battle HUD, ";
    }
    if (config.stop_at_choice){
        ends += "the choice box, ";
    }
    return ends + (party_bar_is_control(config) ? "the controls hint, the party bar or the A prompt" : "the controls hint");
}


//  stop_at_battle's lock-on, since the last text or choice.
struct LockOnState{
    bool after_fade = false;
    Pokemon::HoldTimer quiet_hold{LOCK_ON_AFTER};
    Pokemon::HoldTimer control_hold{CONTROL_HOLD};
    size_t attempts = 0;
    bool locked_on = false;
};

//  The time-dependent part of the machine; a look's time is its `when`.
//  classify() reads the state; record() then adds the look to it.
class SceneTimeline{
public:
    SceneTimeline(SceneMachineConfig config, WallClock start)
        : m_config(std::move(config))
        , m_start(start)
        , m_battle(battle_filter())
    {}

    SceneScreen classify(const ScreenObservation& look) const;
    void record(SceneScreen screen, const ScreenObservation& look);

    //  B, not A: on a look with a sign of control and once one has shown
    //  in the scene, A starts a talk. None: the page's A is pressed.
    std::optional<Button> text_button(const ScreenObservation& look) const;
    //  None after a battle's fade: its effects play with no HUD while the
    //  camera looks away (03.02b's Tepig battle), and A there is a move.
    //  None with stop_at_choice: an A in flight as the choice opens takes
    //  its top row (02.05's elevator closed, console B 2026-11-19 17:37).
    std::optional<Button> other_button() const;

    size_t lock_ons() const{
        return m_lock_on.attempts;
    }
    void lock_on_seen(){
        m_lock_on.locked_on = true;
    }
    size_t black_fades() const{
        return m_black_fades;
    }

private:
    SceneScreen classify_rest(const ScreenObservation& look) const;
    //  text_a_to_choice: a new page (its advance arrow came or went, or
    //  other screens since), or the last A's page TEXT_A_AGAIN on.
    bool text_a_due(const ScreenObservation& look) const;
    SceneScreen classify_control(const ScreenObservation& look) const;
    bool lock_on_due(const ScreenObservation& look) const;
    void record_battle(SceneScreen screen, const ScreenObservation& look);

    SceneMachineConfig m_config;
    WallClock m_start;
    bool m_scene_seen = false;
    //  A sign of control after the scene started.
    bool m_overworld_seen = false;
    Pokemon::HoldTimer m_control_hold{CONTROL_HOLD};
    Pokemon::HoldTimer m_control_instead_hold{CONTROL_INSTEAD_HOLD};
    size_t m_black_fades = 0;
    LockOnState m_lock_on;
    Pokemon::FlickerFilter m_battle;
    //  text_a_to_choice: the last A on text, and whether its page showed the arrow.
    struct TextA{
        WallClock at;
        bool arrow;
    };
    std::optional<TextA> m_text_a;
};

SceneScreen SceneTimeline::classify(const ScreenObservation& look) const{
    const bool battle = look.events.has(ScreenEvent::BATTLE_STARTED);
    if ((m_config.stop_at_battle && battle) || (m_config.end_at_battle && m_battle.on_with(battle, look.when))){
        return SceneScreen::BATTLE;
    }
    if (m_lock_on.locked_on){
        return SceneScreen::LOCKED_ON;
    }
    if (FADE_SHOWS.matches(look)){
        return m_config.end_on_black_fade && look.events.has(ScreenEvent::FADE_BLACK) ? SceneScreen::BLACK_FADE_END : SceneScreen::FADE;
    }
    if (look.events.has(ScreenEvent::CHOICE_BOX)){
        if (m_config.stop_at_choice){
            return SceneScreen::CHOICE_END;
        }
        return SceneScreen::CHOICE;
    }
    if (TEXT_SHOWS.matches(look)){
        return SceneScreen::TEXT;
    }
    if (look.readings.contains(MAP_READING)){
        return SceneScreen::MAP;
    }
    if (lock_on_due(look)){
        return SceneScreen::LOCK_ON;
    }
    return classify_rest(look);
}

SceneScreen SceneTimeline::classify_rest(const ScreenObservation& look) const{
    if (CARD_SHOWS.matches(look)){
        return SceneScreen::CARD;
    }
    if (CONTROL_SHOWS.matches(look)){
        return classify_control(look);
    }
    return SceneScreen::OTHER;
}

SceneScreen SceneTimeline::classify_control(const ScreenObservation& look) const{
    if (m_config.start_window && !m_scene_seen){
        return look.when - m_start >= *m_config.start_window ? SceneScreen::NEVER_STARTED : SceneScreen::AWAITING_SCENE;
    }
    if (m_control_hold.held(look.when)){
        return SceneScreen::CONTROL;
    }
    if (m_control_instead_hold.held(look.when)){
        return SceneScreen::ENDED_IN_CONTROL;
    }
    return SceneScreen::CONTROL_SHOWING;
}

bool SceneTimeline::lock_on_due(const ScreenObservation& look) const{
    if (!m_config.stop_at_battle || !m_config.lock_on || !m_lock_on.after_fade || m_lock_on.attempts >= LOCK_ON_ATTEMPTS || SCENE_BUSY.matches(look)){
        return false;
    }
    return m_lock_on.quiet_hold.held(look.when) ||
        (look.events.has(ScreenEvent::PLAYER_CAN_MOVE) && m_lock_on.control_hold.held(look.when));
}

std::optional<Button> SceneTimeline::text_button(const ScreenObservation& look) const{
    if (m_config.stop_at_choice && !m_config.text_a_to_choice){
        return BUTTON_B;
    }
    if (m_config.stop_at_choice){
        return text_a_due(look) ? std::optional<Button>(BUTTON_A) : std::nullopt;
    }
    return hud_shows(look) || m_overworld_seen ? BUTTON_B : m_config.button;
}

bool SceneTimeline::text_a_due(const ScreenObservation& look) const{
    return !m_text_a || m_text_a->arrow != look.readings.contains(ADVANCE_ARROW_READING) ||
        look.when - m_text_a->at >= TEXT_A_AGAIN;
}

std::optional<Button> SceneTimeline::other_button() const{
    if (m_config.stop_at_choice || (m_config.stop_at_battle && m_lock_on.after_fade)){
        return std::nullopt;
    }
    return m_overworld_seen ? BUTTON_B : m_config.button;
}

void SceneTimeline::record(SceneScreen screen, const ScreenObservation& look){
    m_battle.record(look.events.has(ScreenEvent::BATTLE_STARTED), look.when);
    record_battle(screen, look);
    if (screen != SceneScreen::TEXT){
        m_text_a.reset();
    }else if (m_config.text_a_to_choice && text_a_due(look)){
        m_text_a = TextA{look.when, look.readings.contains(ADVANCE_ARROW_READING)};
    }
    m_black_fades += look.events.has(ScreenEvent::FADE_BLACK) ? 1 : 0;
    m_overworld_seen = m_overworld_seen || (m_scene_seen && CONTROL_SHOWS.matches(look));
    m_scene_seen = m_scene_seen || scene_under_way(look);
    m_control_hold.update(ends_machine(m_config, look), look.when);
    const bool control_instead = !party_bar_is_control(m_config) && CONTROL_SHOWS.matches(look) && !SCENE_BUSY.matches(look);
    m_control_instead_hold.update(control_instead, look.when);
    m_lock_on.control_hold.update(control_instead, look.when);
}

void SceneTimeline::record_battle(SceneScreen screen, const ScreenObservation& look){
    const bool talk = TEXT_SHOWS.matches(look) || look.events.has(ScreenEvent::CHOICE_BOX);
    m_lock_on.attempts = talk ? 0 : m_lock_on.attempts + (screen == SceneScreen::LOCK_ON ? 1 : 0);
    m_lock_on.after_fade = (m_lock_on.after_fade && !talk) || look.events.has(ScreenEvent::FADE_BLACK);
    //  A cutscene (moving, no party bar) is not quiet.
    const bool settled = look.events.has(ScreenEvent::SCREEN_STILL) || look.events.has(ScreenEvent::PLAYER_CAN_MOVE);
    m_lock_on.quiet_hold.update(m_lock_on.after_fade && !SCENE_BUSY.matches(look) && settled, look.when);
}


std::string describe_look(const ScreenObservation& look){
    const std::pair<ScreenEvent, std::string_view> events[] = {
        {ScreenEvent::FADE_BLACK, "black fade"}, {ScreenEvent::FADE_WHITE, "white fade"},
        {ScreenEvent::DIALOG_BOX, "text box"}, {ScreenEvent::CHOICE_BOX, "choice box"},
        {ScreenEvent::BATTLE_STARTED, "battle"}, {ScreenEvent::PLAYER_CAN_MOVE, "party bar"},
        {ScreenEvent::INTERACT_PROMPT, "A prompt"}, {ScreenEvent::SCREEN_STILL, "still"},
    };
    const std::pair<std::string_view, std::string_view> readings[] = {
        {CONTROLS_HINT_READING, "controls hint"}, {ADVANCE_ARROW_READING, "advance arrow"},
        {MISSION_BANNER_READING, "mission banner"}, {MISSION_COMPLETE_READING, "mission complete"},
        {TUTORIAL_CARD_READING, "tutorial card"}, {MAP_READING, "map"},
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


//  A line per change of screen or readings ("[PLZA label] screen 3: other
//  at 12.5s: still"); repeats are summed up every SUMMARY_EVERY ("other x46
//  over 30.0s, 46 presses").
class SceneLog{
public:
    SceneLog(PlzaBackend& backend, std::string_view label, WallClock start)
        : m_backend(backend)
        , m_label(label)
        , m_start(start)
    {}

    void note(std::string_view text){
        flush();
        m_backend.log(std::format("[PLZA {}] {}", m_label, text));
    }
    void look(SceneScreen screen, std::string_view readings, WallClock when, bool pressed);
    //  Drops the counted looks unless they pressed.
    void flush();

private:
    void write_summary();

    PlzaBackend& m_backend;
    std::string m_label;
    WallClock m_start;
    std::optional<SceneScreen> m_screen;
    size_t m_screen_number = 0;
    std::string m_last_key;
    //  The looks since the last line, all of m_screen.
    size_t m_looks = 0;
    size_t m_presses = 0;
    WallClock m_from = {};
    WallClock m_to = {};
};

void SceneLog::look(SceneScreen screen, std::string_view readings, WallClock when, bool pressed){
    const SceneScreenName& name = name_of(screen);
    std::string key = std::format("{}: {}", name.name, readings);
    if (key != m_last_key){
        flush();
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
        return;
    }
    if (m_looks == 0){
        m_from = when;
    }
    m_to = when;
    m_looks++;
    m_presses += pressed ? 1 : 0;
    if (when - m_from >= SUMMARY_EVERY){
        write_summary();
    }
}

void SceneLog::flush(){
    if (m_presses > 0){
        write_summary();
    }
    m_looks = 0;
    m_presses = 0;
}

void SceneLog::write_summary(){
    const double seconds = std::chrono::duration<double>(m_to - m_from).count();
    m_backend.log_detail(std::format(
        "[PLZA {}] {} x{} over {}s, {} presses", m_label, name_of(*m_screen).name, m_looks, tostr_fixed(seconds, 1), m_presses
    ));
    m_looks = 0;
    m_presses = 0;
}


//  Logs each look, then presses for its screen.
class SceneRunner{
public:
    SceneRunner(PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, WallClock start, std::string_view step)
        : m_backend(backend)
        , m_config(config)
        , m_timeline(config, start)
        , m_log(backend, label, start)
        , m_step(step)
    {}

    const SceneTimeline& timeline() const{
        return m_timeline;
    }
    //  Since the last press.
    const SignalProbe& signals() const{
        return m_signals;
    }
    void finish(){
        m_log.flush();
    }
    void act(SceneScreen screen, const ScreenObservation& look);

private:
    //  The button `screen` takes, none for a wait.
    std::optional<Button> button_for(SceneScreen screen, const ScreenObservation& look);
    //  A choice: Up until the top row is highlighted (03.01's German lobby), then A.
    Button choice_button(const ScreenObservation& look);
    //  "[Signals] <step> text_A|text_B by=arrow|dialog_box ...".
    void log_text_press(Button button, const ScreenObservation& look);
    //  Before all but the first, a quarter turn and L put the camera behind
    //  the player (the backend has no camera stick).
    void lock_on();

    PlzaBackend& m_backend;
    SceneMachineConfig m_config;
    SceneTimeline m_timeline;
    SceneLog m_log;
    std::string m_step;
    SignalProbe m_signals;
};

void SceneRunner::act(SceneScreen screen, const ScreenObservation& look){
    const std::optional<Button> button = button_for(screen, look);
    m_timeline.record(screen, look);
    m_signals.look(look);
    std::string readings = describe_look(look);
    if (look.events.has(ScreenEvent::FADE_BLACK)){
        readings = std::format("{}, black fade {}", readings, m_timeline.black_fades());
    }
    m_log.look(screen, readings, look.when, button.has_value() || screen == SceneScreen::LOCK_ON);
    if (screen == SceneScreen::LOCK_ON){
        lock_on();
        m_signals.restart();
        return;
    }
    if (!button){
        if (!terminal(screen)){
            m_backend.wait(POLL);
        }
        return;
    }
    if (screen == SceneScreen::TEXT){
        log_text_press(*button, look);
    }
    m_backend.press(*button, PRESS_HOLD, PRESS_RELEASE);
    m_signals.restart();
}

std::optional<Button> SceneRunner::button_for(SceneScreen screen, const ScreenObservation& look){
    switch (screen){
    case SceneScreen::CHOICE:
        return choice_button(look);
    case SceneScreen::TEXT:
        return m_timeline.text_button(look);
    case SceneScreen::MAP:
        return BUTTON_B;
    case SceneScreen::CARD:
        return BUTTON_A;
    case SceneScreen::OTHER:
        return m_timeline.other_button();
    default:
        return std::nullopt;
    }
}

Button SceneRunner::choice_button(const ScreenObservation& look){
    const auto rows = look.readings.find(CHOICE_ROWS_READING);
    const auto selected = look.readings.find(CHOICE_SELECTED_READING);
    if (rows == look.readings.end() || selected == look.readings.end()){
        m_log.note("choice: rows not read: A");
        return BUTTON_A;
    }
    if (selected->second > 0){
        m_log.note(std::format("choice: {} rows, row {} highlighted: Up to row 0", rows->second, selected->second));
        return BUTTON_UP;
    }
    m_log.note(std::format("choice: {} rows, row 0 highlighted: A", rows->second));
    return BUTTON_A;
}

void SceneRunner::log_text_press(Button button, const ScreenObservation& look){
    //  Not through m_log: it would cut the look counts short.
    const std::string_view by = look.readings.contains(ADVANCE_ARROW_READING) ? "arrow" : "dialog_box";
    m_backend.log_detail(m_signals.line(m_step, button == BUTTON_B ? "text_B" : "text_A", by));
}

void SceneRunner::lock_on(){
    const size_t attempt = m_timeline.lock_ons();
    if (attempt > 1){
        m_backend.stick(/*degrees=*/90, /*duration=*/150ms, /*release=*/100ms, Pokemon::StoryPace::WALK);
        m_backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
    }
    const bool symbols = (*m_config.lock_on)(LOCK_ON_HOLD);
    if (symbols){
        m_timeline.lock_on_seen();
    }
    const std::string seen = symbols ? "the move symbols showed" : std::format("no move symbols in {} ms", LOCK_ON_HOLD.count());
    m_log.note(std::format("lock-on {}/{}: {}ZL held: {}.", attempt, LOCK_ON_ATTEMPTS, attempt > 1 ? "turned 90 degrees right, L, " : "", seen));
}


//  The signal that ended the machine, for its "[Signals]" line.
std::string_view end_signal(SceneScreen screen, const ScreenObservation& look){
    if (screen != SceneScreen::CONTROL){
        return name_of(screen).name;
    }
    if (look.events.has(ScreenEvent::PLAYER_CAN_MOVE)){
        return "party_bar";
    }
    return look.readings.contains(CONTROLS_HINT_READING) ? "hint" : "prompt";
}

std::optional<Pokemon::StoryFailure> scene_outcome(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, SceneScreen screen
){
    const std::string expected = expected_end(config);
    if (screen == SceneScreen::NEVER_STARTED){
        return Pokemon::StoryFailure{.code = "scene_never_started", .detail = std::format(
            "{}: the scene never started (player not at its trigger?): the HUD up and no text, choice, battle or cutscene for {} ms (expecting {}).",
            label, config.start_window ? config.start_window->count() : 0, expected
        )};
    }
    if (screen == SceneScreen::BATTLE && !config.stop_at_battle){
        backend.log(std::format("[PLZA {}] the battle HUD is up: on to the story battle.", label));
        return battle_on_screen_failure();
    }
    if (screen == SceneScreen::ENDED_IN_CONTROL){
        return Pokemon::StoryFailure{.code = "scene_ended_in_control", .detail = std::format(
            "{}: the scene ended in control, the party bar up {} ms with nothing busy (expecting {}).",
            label, CONTROL_INSTEAD_HOLD.count(), expected
        )};
    }
    backend.log(std::format("[PLZA {}] done on {}.", label, name_of(screen).name));
    return std::nullopt;
}

} // namespace


std::string describe_scene_look(const ScreenObservation& look){
    return describe_look(look);
}

bool text_shows(const ScreenObservation& look){
    return TEXT_SHOWS.matches(look);
}

bool fade_shows(const ScreenObservation& look){
    return FADE_SHOWS.matches(look);
}

bool party_bar_free(const ScreenObservation& look){
    return PARTY_BAR_FREE.matches(look);
}

bool scene_under_way(const ScreenObservation& look){
    if (TEXT_SHOWS.matches(look) || look.events.has(ScreenEvent::CHOICE_BOX) || look.events.has(ScreenEvent::BATTLE_STARTED) ||
        look.readings.contains(TUTORIAL_CARD_READING)
    ){
        return true;
    }
    if (hud_shows(look)){
        return false;
    }
    return FADE_SHOWS.matches(look) || !look.events.has(ScreenEvent::SCREEN_STILL);
}

std::optional<Pokemon::StoryFailure> run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout, std::string_view step
){
    backend.log(std::format("[PLZA {}] start, expecting {}.", label, expected_end(config)));
    ScreenObservation look = backend.observe();
    const WallClock start = look.when;
    SceneRunner runner(backend, label, config, start, step);
    //  Each look waits or presses at least POLL; this bounds a script whose time stands still.
    const size_t max_looks = (size_t)(timeout / POLL) + 1;
    for (size_t looks = 1; ; looks++){
        backend.scope().throw_if_cancelled();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            runner.finish();
            return Pokemon::game_not_running_failure();
        }
        const SceneScreen screen = runner.timeline().classify(look);
        runner.act(screen, look);
        if (terminal(screen)){
            runner.finish();
            backend.log_detail(runner.signals().line(step, "scene_end", end_signal(screen, look)));
            return scene_outcome(backend, label, config, screen);
        }
        if (look.when - start >= timeout || looks >= max_looks){
            runner.finish();
            return Pokemon::StoryFailure{.code = "scene_timeout", .detail = std::format(
                "{}: Did not finish within {} s (expecting {}).", label, std::chrono::duration_cast<Seconds>(timeout).count(), expected_end(config)
            )};
        }
        look = backend.observe();
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
