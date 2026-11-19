/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <format>
#include <functional>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/PrettyPrint.h"
#include "CommonTools/ScreenMachine/ScreenMachine.h"
#include "Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStorySceneMachine.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  In the order classify() tries them.
enum class SceneScreen{
    BATTLE,
    FADE_BLACK,
    FADE_WHITE,
    CHOICE,
    DIALOG,         //  a box or the advance chevron
    MISSION_COMPLETE,
    TUTORIAL_CARD,  //  only A turns its pages
    MISSION_BANNER, //  terminal on its second look
    BANNER_SHOWING, //  its first look (every look, with end_after_black_fades)
    BANNER_PRESS,
    FADE_SETTLED,
    CAN_MOVE,       //  the party bar, held PARTY_BAR_HOLD
    PARTY_BAR,
    CONTROLS_HINT,
    HINT_SHOWING,
    TALK_PROMPT,    //  an A prompt held PROMPT_HOLD, past min_length
    PROMPT_SHOWING,
    LOCKED_ON,
    LOCK_ON,
    ENDED_IN_CONTROL,   //  failing: stop_at_battle/choice, but control held CONTROL_INSTEAD_HOLD
    AWAITING_SCENE,
    NEVER_STARTED,
    QUIET_END,
    BLIND_PRESS,
    SCENE_MOVING,
    SCENE_STILL,
};

//  The time-dependent part of the machine; a look's time is its `when`.
class SceneTimeline{
public:
    SceneTimeline(SceneMachineConfig config, WallClock start);

    SceneScreen classify(const ScreenObservation& look) const;

    void record(const ScreenObservation& look);

    //  Since the last text or choice.
    size_t lock_ons() const{
        return m_lock_ons;
    }
    //  Every later look is LOCKED_ON.
    void lock_on_seen(){
        m_locked_on = true;
    }

    //  "fade 2/3" ("fade 2" without end_after_black_fades).
    std::string black_fade_count() const;

    bool banner_seen() const{
        return m_banner_seen;
    }

private:
    std::optional<WallClock> quiet_end_due() const;

    bool blind_press_due(WallClock now) const;

    bool fades_settled(WallClock now) const;

    //  classify() past the fades and the text.
    SceneScreen classify_open(const ScreenObservation& look) const;
    bool banner_press_due(WallClock now) const;
    bool lock_on_due(const ScreenObservation& look) const;

    SceneMachineConfig m_config;
    WallClock m_quiet_from;
    WallClock m_last_busy;
    WallClock m_start;
    //  The last look busy or with a sign of control.
    WallClock m_last_cue;
    std::optional<WallClock> m_last_blind_press;
    std::optional<WallClock> m_party_bar_since;
    std::optional<WallClock> m_hint_since;
    std::optional<WallClock> m_prompt_since;
    //  With stop_at_battle or stop_at_choice: the party bar with nothing busy.
    std::optional<WallClock> m_control_since;
    bool m_scene_seen = false;
    //  In a row.
    size_t m_banner_looks = 0;
    std::optional<WallClock> m_banner_since;
    size_t m_banner_presses = 0;
    std::optional<WallClock> m_last_banner_press;
    size_t m_black_fades = 0;
    //  Once end_after_black_fades' fades have all lifted.
    std::optional<WallClock> m_settled_since;
    bool m_banner_seen = false;
    //  Since the last text or choice.
    bool m_after_fade = false;
    std::optional<WallClock> m_quiet_after_fade_since;
    size_t m_lock_ons = 0;
    bool m_locked_on = false;
};


//  Held this long is free control (two looks or more); the party bar also
//  shows between a scene's lines, and A on a prompt restarts the talk (01.02).
constexpr Milliseconds CONTROLS_HINT_HOLD = 1000ms;
constexpr Milliseconds PARTY_BAR_HOLD = 1000ms;
constexpr Milliseconds PROMPT_HOLD = 1000ms;
//  With stop_at_battle or stop_at_choice, the party bar this long with
//  nothing busy is the scene over with no battle or choice.
constexpr Milliseconds CONTROL_INSTEAD_HOLD = 5000ms;
//  The player can move 5 s after 01.01's third fade at the latest.
constexpr Milliseconds FADE_SETTLE = 5000ms;
constexpr Milliseconds FADE_WAIT = 30000ms;
constexpr Milliseconds STILL_WAIT = 10000ms;
//  The longest a cutscene may hold one still frame.
constexpr Milliseconds SCENE_STILL_WAIT = 60000ms;
constexpr Milliseconds POLL = 250ms;
//  "MAIN MISSION 01" shows ~3 s; 01.03's "MAIN MISSION 02" stays past 10 s
//  and gets A.
constexpr Milliseconds BANNER_GONE_WAIT = 10000ms;
constexpr size_t BANNER_PRESSES = 3;
constexpr Milliseconds BANNER_PRESS_EVERY = 1000ms;
//  With end_after_black_fades: the Italian banner stays until A.
constexpr Milliseconds BANNER_PRESS_AFTER = 2000ms;
//  The battle HUD shows ~1 s after the fade when the camera faces the
//  opponent; a still screen this long after is a battle off camera (03.02b).
constexpr Milliseconds LOCK_ON_AFTER = 3000ms;
constexpr size_t LOCK_ON_ATTEMPTS = 5;
//  TrainerBattle's attempt_one_attack() looks 1 s, the catches' sweep 1.5 s.
constexpr Milliseconds LOCK_ON_HOLD = 2000ms;

bool busy(const ScreenObservation& look){
    return text_shows(look) || fade_shows(look) || look.readings.contains(MISSION_COMPLETE_READING) ||
        look.readings.contains(TUTORIAL_CARD_READING);
}

bool scene_shows(const ScreenObservation& look){
    return text_shows(look) || look.events.has(ScreenEvent::CHOICE_BOX) || look.events.has(ScreenEvent::BATTLE_STARTED);
}

bool hud_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) || look.readings.contains(CONTROLS_HINT_READING) ||
        look.events.has(ScreenEvent::INTERACT_PROMPT);
}

//  Not when stopping at a choice or a battle: the talk shows them between lines.
bool party_bar_is_control(const SceneMachineConfig& config){
    return !config.stop_at_choice && !config.stop_at_battle;
}

bool control_prompt_shows(const SceneMachineConfig& config, const ScreenObservation& look){
    return party_bar_is_control(config) && look.events.has(ScreenEvent::INTERACT_PROMPT);
}

//  On a closing box B does nothing, where A restarts the talk (03.05's nurse).
Button dialog_button(const SceneMachineConfig& config, const ScreenObservation& look){
    if (config.text_button){
        return *config.text_button;
    }
    return party_bar_is_control(config) && look.events.has(ScreenEvent::DIALOG_BOX) ? BUTTON_B : BUTTON_A;
}

constexpr SceneScreen ALL_SCREENS[] = {
    SceneScreen::BATTLE,
    SceneScreen::FADE_BLACK, SceneScreen::FADE_WHITE, SceneScreen::CHOICE, SceneScreen::DIALOG,
    SceneScreen::MISSION_COMPLETE, SceneScreen::TUTORIAL_CARD,
    SceneScreen::MISSION_BANNER, SceneScreen::BANNER_SHOWING, SceneScreen::BANNER_PRESS, SceneScreen::FADE_SETTLED,
    SceneScreen::CAN_MOVE, SceneScreen::PARTY_BAR,
    SceneScreen::CONTROLS_HINT, SceneScreen::HINT_SHOWING,
    SceneScreen::TALK_PROMPT, SceneScreen::PROMPT_SHOWING,
    SceneScreen::LOCKED_ON, SceneScreen::LOCK_ON, SceneScreen::ENDED_IN_CONTROL, SceneScreen::AWAITING_SCENE, SceneScreen::NEVER_STARTED, SceneScreen::QUIET_END,
    SceneScreen::BLIND_PRESS, SceneScreen::SCENE_MOVING, SceneScreen::SCENE_STILL,
};

void track_since(bool shows, WallClock when, std::optional<WallClock>& since){
    if (!shows){
        since.reset();
    }else if (!since){
        since = when;
    }
}


std::string_view scene_screen_name(SceneScreen screen){
    constexpr std::pair<SceneScreen, std::string_view> NAMES[] = {
        {SceneScreen::BATTLE, "battle"}, {SceneScreen::FADE_BLACK, "fade_black"}, {SceneScreen::FADE_WHITE, "fade_white"},
        {SceneScreen::CHOICE, "choice"}, {SceneScreen::DIALOG, "dialog"}, {SceneScreen::MISSION_COMPLETE, "mission_complete"},
        {SceneScreen::TUTORIAL_CARD, "tutorial_card"}, {SceneScreen::MISSION_BANNER, "mission_banner"},
        {SceneScreen::BANNER_SHOWING, "banner_showing"}, {SceneScreen::BANNER_PRESS, "banner_press"},
        {SceneScreen::FADE_SETTLED, "fade_settled"}, {SceneScreen::CAN_MOVE, "can_move"}, {SceneScreen::PARTY_BAR, "party_bar"},
        {SceneScreen::CONTROLS_HINT, "controls_hint"}, {SceneScreen::HINT_SHOWING, "hint_showing"},
        {SceneScreen::TALK_PROMPT, "talk_prompt"}, {SceneScreen::PROMPT_SHOWING, "prompt_showing"},
        {SceneScreen::LOCKED_ON, "locked_on"}, {SceneScreen::LOCK_ON, "lock_on"},
        {SceneScreen::ENDED_IN_CONTROL, "ended_in_control"}, {SceneScreen::AWAITING_SCENE, "awaiting_scene"},
        {SceneScreen::NEVER_STARTED, "never_started"}, {SceneScreen::QUIET_END, "quiet_end"},
        {SceneScreen::BLIND_PRESS, "blind_press"}, {SceneScreen::SCENE_MOVING, "scene_moving"},
        {SceneScreen::SCENE_STILL, "scene_still"},
    };
    for (const auto& [state, name] : NAMES){
        if (state == screen){
            return name;
        }
    }
    return "unknown";
}

bool scene_screen_terminal(SceneScreen screen){
    return screen == SceneScreen::BATTLE || screen == SceneScreen::CHOICE || screen == SceneScreen::MISSION_BANNER ||
        screen == SceneScreen::CAN_MOVE || screen == SceneScreen::CONTROLS_HINT || screen == SceneScreen::TALK_PROMPT ||
        screen == SceneScreen::FADE_SETTLED || screen == SceneScreen::LOCKED_ON ||
        screen == SceneScreen::NEVER_STARTED || screen == SceneScreen::QUIET_END || screen == SceneScreen::ENDED_IN_CONTROL;
}

bool scene_screen_presses(SceneScreen screen){
    return screen == SceneScreen::DIALOG || screen == SceneScreen::MISSION_COMPLETE || screen == SceneScreen::TUTORIAL_CARD ||
        screen == SceneScreen::BLIND_PRESS || screen == SceneScreen::BANNER_PRESS || screen == SceneScreen::LOCK_ON;
}

//  "the controls hint, the party bar, the A prompt or the mission banner".
std::string expected_end(const SceneMachineConfig& config){
    std::vector<std::string> ends;
    if (config.stop_at_battle){
        ends.push_back("the battle HUD");
    }
    if (config.stop_at_choice){
        ends.push_back("the choice box");
    }
    if (config.end_after_black_fades){
        ends.push_back(std::format("black fade {} settled", *config.end_after_black_fades));
    }
    ends.push_back("the controls hint");
    if (party_bar_is_control(config)){
        ends.push_back("the party bar");
        ends.push_back("the A prompt");
    }
    if (!config.end_after_black_fades){
        ends.push_back("the mission banner");
    }
    if (config.quiet_is_control){
        ends.push_back("quiet");
    }
    std::string text;
    for (size_t index = 0; index < ends.size(); index++){
        const std::string_view separator = index == 0 ? "" : index + 1 == ends.size() ? " or " : ", ";
        text += std::format("{}{}", separator, ends[index]);
    }
    return text;
}

//  The states logged on the overlay too; the rest go to the file log only.
bool scene_screen_for_watcher(SceneScreen screen){
    return screen == SceneScreen::FADE_BLACK || screen == SceneScreen::BANNER_SHOWING || screen == SceneScreen::BANNER_PRESS ||
        screen == SceneScreen::HINT_SHOWING;
}


SceneTimeline::SceneTimeline(SceneMachineConfig config, WallClock start)
    : m_config(std::move(config))
    , m_quiet_from(start + m_config.min_length)
    , m_last_busy(start)
    , m_start(start)
    , m_last_cue(start)
{}

std::optional<WallClock> SceneTimeline::quiet_end_due() const{
    if (!m_config.quiet){
        return std::nullopt;
    }
    return std::max(m_quiet_from, m_last_busy + *m_config.quiet);
}

bool SceneTimeline::blind_press_due(WallClock now) const{
    const bool pressed_since_cue = m_last_blind_press && *m_last_blind_press >= m_last_cue;
    if (m_config.gap_press && m_scene_seen && !pressed_since_cue && now - m_last_cue >= *m_config.gap_press){
        return true;
    }
    if (!m_config.blind_press_after || now - m_last_cue < *m_config.blind_press_after){
        return false;
    }
    return !m_last_blind_press || now - *m_last_blind_press >= m_config.blind_press_every;
}

bool SceneTimeline::fades_settled(WallClock now) const{
    return m_settled_since && now - *m_settled_since >= FADE_SETTLE;
}

bool SceneTimeline::banner_press_due(WallClock now) const{
    if (!m_config.end_after_black_fades || !m_banner_since || now - *m_banner_since < BANNER_PRESS_AFTER){
        return false;
    }
    return m_banner_presses < BANNER_PRESSES && (!m_last_banner_press || now - *m_last_banner_press >= BANNER_PRESS_EVERY);
}

bool SceneTimeline::lock_on_due(const ScreenObservation& look) const{
    if (!m_config.stop_at_battle || !m_config.lock_on || !m_after_fade || m_lock_ons >= LOCK_ON_ATTEMPTS || busy(look)){
        return false;
    }
    return (m_quiet_after_fade_since && look.when - *m_quiet_after_fade_since >= LOCK_ON_AFTER) ||
        (m_control_since && look.events.has(ScreenEvent::PLAYER_CAN_MOVE) && look.when - *m_control_since >= PARTY_BAR_HOLD);
}

std::string SceneTimeline::black_fade_count() const{
    if (!m_config.end_after_black_fades){
        return std::format("fade {}", m_black_fades);
    }
    return std::format("fade {}/{}", m_black_fades, *m_config.end_after_black_fades);
}

SceneScreen SceneTimeline::classify(const ScreenObservation& look) const{
    if (m_config.stop_at_battle && look.events.has(ScreenEvent::BATTLE_STARTED)){
        return SceneScreen::BATTLE;
    }
    if (m_locked_on){
        return SceneScreen::LOCKED_ON;
    }
    if (look.events.has(ScreenEvent::FADE_BLACK)){
        return SceneScreen::FADE_BLACK;
    }
    if (look.events.has(ScreenEvent::FADE_WHITE)){
        return SceneScreen::FADE_WHITE;
    }
    if (m_config.stop_at_choice && look.events.has(ScreenEvent::CHOICE_BOX)){
        return SceneScreen::CHOICE;
    }
    if (text_shows(look)){
        return SceneScreen::DIALOG;
    }
    if (lock_on_due(look)){
        return SceneScreen::LOCK_ON;
    }
    return classify_open(look);
}

SceneScreen SceneTimeline::classify_open(const ScreenObservation& look) const{
    if (look.readings.contains(MISSION_COMPLETE_READING)){
        return SceneScreen::MISSION_COMPLETE;
    }
    if (look.readings.contains(TUTORIAL_CARD_READING)){
        return SceneScreen::TUTORIAL_CARD;
    }
    if (look.readings.contains(MISSION_BANNER_READING)){
        if (!m_config.end_after_black_fades && m_banner_looks >= 1){
            return SceneScreen::MISSION_BANNER;
        }
        return banner_press_due(look.when) ? SceneScreen::BANNER_PRESS : SceneScreen::BANNER_SHOWING;
    }
    if (fades_settled(look.when)){
        return SceneScreen::FADE_SETTLED;
    }
    if (m_control_since && look.events.has(ScreenEvent::PLAYER_CAN_MOVE) && look.when - *m_control_since >= CONTROL_INSTEAD_HOLD){
        return SceneScreen::ENDED_IN_CONTROL;
    }
    if (m_config.start_window && !m_scene_seen && hud_shows(look)){
        const bool past_window = look.when - m_start >= *m_config.start_window;
        return past_window ? SceneScreen::NEVER_STARTED : SceneScreen::AWAITING_SCENE;
    }
    if (party_bar_is_control(m_config) && look.events.has(ScreenEvent::PLAYER_CAN_MOVE)){
        if (m_party_bar_since && look.when - *m_party_bar_since >= PARTY_BAR_HOLD){
            return SceneScreen::CAN_MOVE;
        }
        return SceneScreen::PARTY_BAR;
    }
    if (look.readings.contains(CONTROLS_HINT_READING)){
        if (m_hint_since && look.when - *m_hint_since >= CONTROLS_HINT_HOLD){
            return SceneScreen::CONTROLS_HINT;
        }
        return SceneScreen::HINT_SHOWING;
    }
    if (control_prompt_shows(m_config, look)){
        if (m_prompt_since && look.when - *m_prompt_since >= PROMPT_HOLD && look.when >= m_quiet_from){
            return SceneScreen::TALK_PROMPT;
        }
        return SceneScreen::PROMPT_SHOWING;
    }
    const std::optional<WallClock> due = quiet_end_due();
    if (due && look.when >= *due){
        return SceneScreen::QUIET_END;
    }
    if (blind_press_due(look.when)){
        return SceneScreen::BLIND_PRESS;
    }
    return look.events.has(ScreenEvent::SCREEN_STILL) ? SceneScreen::SCENE_STILL : SceneScreen::SCENE_MOVING;
}

void SceneTimeline::record(const ScreenObservation& look){
    const SceneScreen screen = classify(look);
    if (screen == SceneScreen::BLIND_PRESS){
        m_last_blind_press = look.when;
    }
    if (screen == SceneScreen::BANNER_PRESS){
        m_banner_presses++;
        m_last_banner_press = look.when;
    }
    const bool talk = text_shows(look) || look.events.has(ScreenEvent::CHOICE_BOX);
    m_lock_ons = talk ? 0 : m_lock_ons + (screen == SceneScreen::LOCK_ON ? 1 : 0);
    m_after_fade = (m_after_fade && !talk) || look.events.has(ScreenEvent::FADE_BLACK);
    //  A cutscene (moving, no party bar) is not quiet.
    const bool settled = look.events.has(ScreenEvent::SCREEN_STILL) || look.events.has(ScreenEvent::PLAYER_CAN_MOVE);
    track_since(m_after_fade && !busy(look) && settled, look.when, m_quiet_after_fade_since);
    if (busy(look)){
        m_last_busy = look.when;
    }
    m_scene_seen = m_scene_seen || scene_under_way(look);
    const bool box = look.events.has(ScreenEvent::DIALOG_BOX);
    track_since(party_bar_is_control(m_config) && party_bar_free(look), look.when, m_party_bar_since);
    const bool control_instead = !party_bar_is_control(m_config) && look.events.has(ScreenEvent::PLAYER_CAN_MOVE) && !busy(look);
    track_since(control_instead, look.when, m_control_since);
    track_since(look.readings.contains(CONTROLS_HINT_READING) && !box, look.when, m_hint_since);
    track_since(control_prompt_shows(m_config, look) && !box, look.when, m_prompt_since);
    m_banner_looks = look.readings.contains(MISSION_BANNER_READING) && !box ? m_banner_looks + 1 : 0;
    m_banner_seen = m_banner_seen || m_banner_looks > 0;
    track_since(m_banner_looks > 0, look.when, m_banner_since);
    if (!m_banner_since){
        m_banner_presses = 0;
        m_last_banner_press.reset();
    }
    //  Each FADE_BLACK look is a new fade: its state waits until it lifts.
    m_black_fades += look.events.has(ScreenEvent::FADE_BLACK) ? 1 : 0;
    const bool fades_done = m_config.end_after_black_fades && m_black_fades >= *m_config.end_after_black_fades;
    track_since(fades_done && !busy(look) && m_banner_looks == 0, look.when, m_settled_since);
    if (busy(look) || m_party_bar_since || m_hint_since || m_prompt_since || m_banner_looks > 0){
        m_last_cue = look.when;
    }
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

constexpr Milliseconds SUMMARY_EVERY = 30000ms;
//  How many states may take turns and still count as one run of looks.
constexpr std::ptrdiff_t TURN_PERIODS[] = {2, 3};

//  A line per change of state or readings ("[PLZA label] screen 3:
//  scene_still at 12.5s: still"); repeats and two or three states taking
//  turns are summed up every SUMMARY_EVERY ("scene_moving x46, blind_press x15
//  over 30.0s, 15 presses").
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

    void look(SceneScreen screen, std::string_view readings, WallClock when, bool pressed){
        if (screen != m_screen){
            m_screen = screen;
            m_screen_number++;
        }
        if (gets_line(std::format("{}: {}", scene_screen_name(screen), readings))){
            flush();
            const double seconds = std::chrono::duration<double>(when - m_start).count();
            const std::string line = std::format(
                "[PLZA {}] screen {}: {} at {}s: {}",
                m_label, m_screen_number, scene_screen_name(screen), tostr_fixed(seconds, 1), readings
            );
            if (scene_screen_for_watcher(screen)){
                m_backend.log(line);
            }else{
                m_backend.log_detail(line);
            }
            return;
        }
        count(screen, when, pressed);
        if (when - *m_counted_from >= SUMMARY_EVERY){
            write_summary();
        }
    }

    //  Drops the counted looks unless they took turns or pressed.
    void flush(){
        if (m_counted.size() > 1 || m_presses > 0){
            write_summary();
            return;
        }
        m_counted.clear();
        m_counted_from.reset();
    }

private:
    void write_summary(){
        std::string text;
        for (const auto& [screen, looks] : m_counted){
            text += std::format("{}{} x{}", text.empty() ? "" : ", ", scene_screen_name(screen), looks);
        }
        const double seconds = std::chrono::duration<double>(m_counted_to - *m_counted_from).count();
        m_backend.log_detail(std::format("[PLZA {}] {} over {}s, {} presses", m_label, text, tostr_fixed(seconds, 1), m_presses));
        m_counted.clear();
        m_counted_from.reset();
        m_presses = 0;
    }

    //  `key`: "scene_still: still".
    bool gets_line(std::string key){
        if (!m_changes.empty() && key == m_changes.back()){
            return false;
        }
        m_changes.push_back(std::move(key));
        if (m_changes.size() > MAX_CHANGES){
            m_changes.erase(m_changes.begin());
        }
        return !takes_turns();
    }

    //  The last 4 or 6 changes are one run of 2 or 3 repeated.
    bool takes_turns() const{
        for (const std::ptrdiff_t period : TURN_PERIODS){
            if ((std::ptrdiff_t)m_changes.size() >= 2 * period &&
                std::equal(m_changes.end() - period, m_changes.end(), m_changes.end() - 2 * period)
            ){
                return true;
            }
        }
        return false;
    }

    void count(SceneScreen screen, WallClock when, bool pressed){
        if (!m_counted_from){
            m_counted_from = when;
        }
        m_counted_to = when;
        m_presses += pressed ? 1 : 0;
        for (auto& [counted, looks] : m_counted){
            if (counted == screen){
                looks++;
                return;
            }
        }
        m_counted.emplace_back(screen, 1);
    }

    PlzaBackend& m_backend;
    std::string m_label;
    WallClock m_start;
    std::optional<SceneScreen> m_screen;
    size_t m_screen_number = 0;
    static constexpr size_t MAX_CHANGES = 6;
    std::vector<std::string> m_changes;
    std::vector<std::pair<SceneScreen, size_t>> m_counted;
    std::optional<WallClock> m_counted_from;
    WallClock m_counted_to;
    size_t m_presses = 0;
};

class SceneRunner{
public:
    SceneRunner(PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, WallClock start)
        : m_backend(backend)
        , m_config(config)
        , m_timeline(config, start)
        , m_log(backend, label, start)
    {}

    const SceneTimeline& timeline() const{
        return m_timeline;
    }

    void flush_log(){
        m_log.flush();
    }

    void act(SceneScreen screen, const ScreenObservation& look){
        m_timeline.record(look);
        std::string readings = describe_look(look);
        if (screen == SceneScreen::FADE_BLACK){
            readings = std::format("{}, {}", readings, m_timeline.black_fade_count());
        }
        m_log.look(screen, readings, look.when, scene_screen_presses(screen));
        if (scene_screen_terminal(screen)){
            return;
        }
        if (screen == SceneScreen::LOCK_ON){
            lock_on();
            return;
        }
        if (scene_screen_presses(screen)){
            m_backend.press(button_of(screen, look), /*hold=*/160ms, /*release=*/240ms);
            return;
        }
        wait(screen);
    }

private:
    //  Before all but the first, a quarter turn and L put the camera behind
    //  the player (the backend has no camera stick).
    void lock_on(){
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

    Button button_of(SceneScreen screen, const ScreenObservation& look){
        switch (screen){
        case SceneScreen::DIALOG:
            return look.events.has(ScreenEvent::CHOICE_BOX) ? choice_button(look) : dialog_button(m_config, look);
        case SceneScreen::BLIND_PRESS:
            return m_config.blind_button;
        default:
            return BUTTON_A;
        }
    }

    //  Up until the top row is highlighted (03.01's German lobby), then A.
    Button choice_button(const ScreenObservation& look){
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

    void wait(SceneScreen screen){
        switch (screen){
        case SceneScreen::FADE_BLACK:
        case SceneScreen::FADE_WHITE:
            wait_until(FADE_WAIT, [](const ScreenObservation& look){
                return !fade_shows(look);
            });
            return;
        case SceneScreen::SCENE_STILL:
            //  Also ends on a still screen with the chevron (01.01's subtitles).
            wait_until(STILL_WAIT, [this](const ScreenObservation& look){
                return m_timeline.classify(look) != SceneScreen::SCENE_STILL;
            });
            return;
        case SceneScreen::PARTY_BAR:
        case SceneScreen::HINT_SHOWING:
        case SceneScreen::PROMPT_SHOWING:
        case SceneScreen::AWAITING_SCENE:
        case SceneScreen::BANNER_SHOWING:
            m_backend.wait(POLL);
            return;
        default:
            m_backend.wait(2 * POLL);
            return;
        }
    }

    template <typename Done>
    void wait_until(Milliseconds limit, Done&& done){
        for (Milliseconds waited = 0ms; waited < limit; waited += POLL){
            m_backend.wait(POLL);
            if (done(m_backend.observe())){
                return;
            }
        }
    }

    PlzaBackend& m_backend;
    SceneMachineConfig m_config;
    SceneTimeline m_timeline;
    SceneLog m_log;
};

//  A box or a card that A does not get past fails.
size_t max_visits(SceneScreen screen){
    switch (screen){
    case SceneScreen::DIALOG:
        return 400;
    case SceneScreen::MISSION_COMPLETE:
        return 30;
    case SceneScreen::BANNER_SHOWING:
        //  ~30 s: an uncleared banner's frames change too much for max_still.
        return 120;
    default:
        return SIZE_MAX;
    }
}

//  How long a state may see an unchanged screen; zero for DIALOG, whose
//  presses must change it.
Milliseconds max_still(SceneScreen screen){
    switch (screen){
    case SceneScreen::FADE_BLACK:
    case SceneScreen::FADE_WHITE:
        return FADE_WAIT;
    case SceneScreen::SCENE_STILL:
    case SceneScreen::BLIND_PRESS:
    case SceneScreen::BANNER_PRESS:
    case SceneScreen::LOCK_ON:
    case SceneScreen::PROMPT_SHOWING:
    case SceneScreen::AWAITING_SCENE:
        //  Presses here need not change the screen.
        return SCENE_STILL_WAIT;
    case SceneScreen::PARTY_BAR:
    case SceneScreen::HINT_SHOWING:
    case SceneScreen::BANNER_SHOWING:
        return STILL_WAIT;
    default:
        return Milliseconds::zero();
    }
}

bool banner_shows(PlzaBackend& backend){
    return backend.observe().readings.contains(MISSION_BANNER_READING);
}

//  So the next step starts with control.
void wait_banner_gone(PlzaBackend& backend, std::string_view label){
    for (Milliseconds waited = 0ms; waited < BANNER_GONE_WAIT; waited += POLL){
        if (!banner_shows(backend)){
            backend.log(std::format("[PLZA {}] the mission banner has gone after {} ms.", label, waited.count()));
            return;
        }
        backend.wait(POLL);
    }
    backend.log(std::format("[PLZA {}] the mission banner still shows after {} ms; pressing A.", label, BANNER_GONE_WAIT.count()));
    for (size_t press = 1; press <= BANNER_PRESSES; press++){
        backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
        backend.wait(BANNER_PRESS_EVERY);
        if (!banner_shows(backend)){
            backend.log(std::format("[PLZA {}] the mission banner has gone after A press {}.", label, press));
            return;
        }
    }
    backend.log(std::format("[PLZA {}] the mission banner still shows after {} A presses; going on.", label, BANNER_PRESSES));
}

Pokemon::Outcome scene_outcome(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, const ScreenMachineResult& result
){
    const std::string expected = expected_end(config);
    if (!result.succeeded()){
        const std::string_view kind = result.failure.starts_with("Did not finish") ? "scene_timeout" : "scene_stuck";
        return Pokemon::outcome_failed(std::string(kind), std::format("{}: {} (expecting {}).", label, result.failure, expected));
    }
    if (result.terminal == scene_screen_name(SceneScreen::NEVER_STARTED)){
        return Pokemon::outcome_failed("scene_never_started", std::format(
            "{}: the scene never started (player not at its trigger?): the HUD up and no text, choice, battle or cutscene for {} ms (expecting {}).",
            label, config.start_window ? config.start_window->count() : 0, expected
        ));
    }
    if (result.terminal == scene_screen_name(SceneScreen::ENDED_IN_CONTROL)){
        return Pokemon::outcome_failed("scene_ended_in_control", std::format(
            "{}: the scene ended in control, the party bar up {} ms with nothing busy (expecting {}).",
            label, CONTROL_INSTEAD_HOLD.count(), expected
        ));
    }
    if (result.terminal == scene_screen_name(SceneScreen::QUIET_END) && config.quiet_is_control){
        backend.log(std::format("[PLZA {}] done: quiet, taken as control.", label));
        return Pokemon::outcome_ok();
    }
    if (result.terminal == scene_screen_name(SceneScreen::QUIET_END)){
        //  A cutscene with no text, or control the detectors miss.
        return Pokemon::outcome_failed("scene_quiet", std::format(
            "{}: nothing busy for {} ms and no sign of control (expecting {}).", label, config.quiet ? config.quiet->count() : 0, expected
        ));
    }
    if (result.terminal == scene_screen_name(SceneScreen::MISSION_BANNER)){
        wait_banner_gone(backend, label);
    }
    backend.log(std::format("[PLZA {}] done on {}.", label, result.terminal));
    return Pokemon::outcome_ok();
}

} // namespace


std::string describe_scene_look(const ScreenObservation& look){
    return describe_look(look);
}

bool text_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::DIALOG_BOX) || look.readings.contains(ADVANCE_ARROW_READING);
}

bool fade_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::FADE_BLACK) || look.events.has(ScreenEvent::FADE_WHITE);
}

bool party_bar_free(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) && !look.events.has(ScreenEvent::DIALOG_BOX);
}

bool scene_under_way(const ScreenObservation& look){
    if (scene_shows(look) || look.readings.contains(TUTORIAL_CARD_READING)){
        return true;
    }
    if (hud_shows(look)){
        return false;
    }
    return fade_shows(look) || !look.events.has(ScreenEvent::SCREEN_STILL);
}

Pokemon::Outcome run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout
){
    backend.log(std::format("[PLZA {}] start, expecting {}.", label, expected_end(config)));
    Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
    bool first = true;
    SceneRunner runner(backend, label, config, look.when);
    std::vector<ScreenState> states;
    for (const SceneScreen screen : ALL_SCREENS){
        ScreenState state;
        state.name = std::string(scene_screen_name(screen));
        state.matches = [&runner, screen](const ScreenObservation& observation){
            return runner.timeline().classify(observation) == screen;
        };
        state.act = [&runner, &look, screen]{
            runner.act(screen, look);
        };
        state.max_visits = max_visits(screen);
        state.max_still = max_still(screen);
        state.terminal = scene_screen_terminal(screen);
        states.push_back(std::move(state));
    }
    const std::function<ScreenObservation()> observe = [&backend, &look, &first]() -> ScreenObservation{
        if (!first){
            look = backend.observe();
        }
        first = false;
        return look;
    };
    const ScreenMachineResult result = run_screen_machine(observe, backend.scope(), states, timeout, /*log=*/nullptr);
    runner.flush_log();
    if (config.end_after_black_fades){
        backend.log(std::format(
            "[PLZA {}] the mission banner {}.", label, runner.timeline().banner_seen() ? "showed" : "did not show (the detector may have missed it)"
        ));
    }
    return scene_outcome(backend, label, config, result);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
