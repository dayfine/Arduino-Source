/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "CommonTools/ScreenMachine/ScreenMachine.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStorySceneMachine.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  The states a scene machine tells apart, in the order it tries them.
enum class SceneScreen{
    BATTLE,         //  terminal: the battle HUD, with stop_at_battle
    FADE_BLACK,     //  wait until it lifts
    FADE_WHITE,     //  wait until it lifts
    CHOICE,         //  terminal: the choice box, with stop_at_choice
    DIALOG,         //  the text button: a box (A: its choice's top row too), or the advance chevron
    MISSION_COMPLETE,   //  A: a main mission's "COMPLETE" card
    MISSION_BANNER, //  terminal: a main mission's banner, on a second look
    BANNER_SHOWING, //  wait: the banner, on its first look
    CAN_MOVE,       //  terminal: the party bar, held a second
    PARTY_BAR,      //  wait: the party bar, not held a second yet
    CONTROLS_HINT,  //  terminal: the tutorial's controls hint, held a second
    HINT_SHOWING,   //  wait: the controls hint, not held a second yet
    NEVER_STARTED,  //  terminal, failing: no scene within start_window
    QUIET_END,      //  terminal, failing unless quiet_is_control: nothing busy for the quiet time, past the minimum length
    BLIND_PRESS,    //  blind_button: nothing busy and no sign of control for blind_press_after
    SCENE_MOVING,   //  wait: no text, the screen changing
    SCENE_STILL,    //  wait: no text, the screen still since the look before
};

//  The time-dependent part of the machine: when the screen was last busy,
//  since when the hint shows. A look's time is its `when`.
class SceneTimeline{
public:
    SceneTimeline(SceneMachineConfig config, WallClock start);

    //  The state for `look`. Does not change the timeline.
    SceneScreen classify(const ScreenObservation& look) const;

    //  Takes in the look the machine acted on.
    void record(const ScreenObservation& look);

    //  When the quiet end is due if nothing busy shows before; none
    //  without a quiet time.
    std::optional<WallClock> quiet_end_due() const;

    //  Whether a blind press is due at `now` (blind_press_after).
    bool blind_press_due(WallClock now) const;

private:
    //  classify() past the fades and the text.
    SceneScreen classify_open(const ScreenObservation& look) const;

    SceneMachineConfig m_config;
    WallClock m_quiet_from;
    WallClock m_last_busy;
    WallClock m_start;
    //  The last look busy or with a sign of control (party bar, hint,
    //  banner).
    WallClock m_last_cue;
    std::optional<WallClock> m_last_blind_press;
    std::optional<WallClock> m_party_bar_since;
    std::optional<WallClock> m_hint_since;
    //  Text, a choice or a battle has shown: the scene has started.
    bool m_scene_seen = false;
    //  The looks in a row with the mission banner.
    size_t m_banner_looks = 0;
};


//  The controls hint showing this long is free control: two looks or more.
constexpr Milliseconds CONTROLS_HINT_HOLD = 1000ms;
//  The party bar showing this long is free control: it also shows between
//  the lines of a scene.
constexpr Milliseconds PARTY_BAR_HOLD = 1000ms;
//  The longest a fade is waited out in one step.
constexpr Milliseconds FADE_WAIT = 30000ms;
//  The longest a still screen is waited on in one step.
constexpr Milliseconds STILL_WAIT = 10000ms;
//  The longest a cutscene may hold one still frame.
constexpr Milliseconds SCENE_STILL_WAIT = 60000ms;
//  How often blind_press_after presses.
constexpr Milliseconds BLIND_PRESS_EVERY = 2000ms;
//  How often a wait looks again.
constexpr Milliseconds POLL = 250ms;
//  The longest the machine waits for the mission banner to go once it has
//  ended the scene ("MAIN MISSION 01" shows ~3 s).
constexpr Milliseconds BANNER_GONE_WAIT = 10000ms;
//  A banner still up after BANNER_GONE_WAIT gets A this many times, this
//  far apart, until it goes: 01.03's "MAIN MISSION 02" stayed past 10 s
//  (console B, 2026-11-10).
constexpr size_t BANNER_PRESSES = 3;
constexpr Milliseconds BANNER_PRESS_EVERY = 1000ms;

//  Text to press on: a box, or the advance chevron without one.
bool text_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::DIALOG_BOX) || look.readings.contains(ADVANCE_ARROW_READING);
}

bool fade_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::FADE_BLACK) || look.events.has(ScreenEvent::FADE_WHITE);
}

//  Something holds the player: text, a fade, or the mission complete card.
bool busy(const ScreenObservation& look){
    return text_shows(look) || fade_shows(look) || look.readings.contains(MISSION_COMPLETE_READING);
}

//  A scene has started: text, a choice or a battle.
bool scene_shows(const ScreenObservation& look){
    return text_shows(look) || look.events.has(ScreenEvent::CHOICE_BOX) || look.events.has(ScreenEvent::BATTLE_STARTED);
}

//  Whether the party bar is control for `config`: not in a machine that
//  stops at a choice or a battle, whose talk shows it between its lines.
bool party_bar_is_control(const SceneMachineConfig& config){
    return !config.stop_at_choice && !config.stop_at_battle;
}

constexpr SceneScreen ALL_SCREENS[] = {
    SceneScreen::BATTLE,
    SceneScreen::FADE_BLACK, SceneScreen::FADE_WHITE, SceneScreen::CHOICE, SceneScreen::DIALOG,
    SceneScreen::MISSION_COMPLETE,
    SceneScreen::MISSION_BANNER, SceneScreen::BANNER_SHOWING,
    SceneScreen::CAN_MOVE, SceneScreen::PARTY_BAR,
    SceneScreen::CONTROLS_HINT, SceneScreen::HINT_SHOWING,
    SceneScreen::NEVER_STARTED, SceneScreen::QUIET_END, SceneScreen::BLIND_PRESS,
    SceneScreen::SCENE_MOVING, SceneScreen::SCENE_STILL,
};

//  Starts `since` when `shows` starts, clears it when it stops.
void track_since(bool shows, WallClock when, std::optional<WallClock>& since){
    if (!shows){
        since.reset();
    }else if (!since){
        since = when;
    }
}


//  "fade_black", the state's name in the StepTrace log.
std::string_view scene_screen_name(SceneScreen screen){
    switch (screen){
    case SceneScreen::BATTLE:
        return "battle";
    case SceneScreen::FADE_BLACK:
        return "fade_black";
    case SceneScreen::FADE_WHITE:
        return "fade_white";
    case SceneScreen::CHOICE:
        return "choice";
    case SceneScreen::DIALOG:
        return "dialog";
    case SceneScreen::MISSION_COMPLETE:
        return "mission_complete";
    case SceneScreen::MISSION_BANNER:
        return "mission_banner";
    case SceneScreen::BANNER_SHOWING:
        return "banner_showing";
    case SceneScreen::CAN_MOVE:
        return "can_move";
    case SceneScreen::PARTY_BAR:
        return "party_bar";
    case SceneScreen::CONTROLS_HINT:
        return "controls_hint";
    case SceneScreen::HINT_SHOWING:
        return "hint_showing";
    case SceneScreen::NEVER_STARTED:
        return "never_started";
    case SceneScreen::QUIET_END:
        return "quiet_end";
    case SceneScreen::BLIND_PRESS:
        return "blind_press";
    case SceneScreen::SCENE_MOVING:
        return "scene_moving";
    case SceneScreen::SCENE_STILL:
        return "scene_still";
    }
    return "unknown";
}

//  Whether the machine ends after this state's step.
bool scene_screen_terminal(SceneScreen screen){
    return screen == SceneScreen::BATTLE || screen == SceneScreen::CHOICE || screen == SceneScreen::MISSION_BANNER ||
        screen == SceneScreen::CAN_MOVE || screen == SceneScreen::CONTROLS_HINT ||
        screen == SceneScreen::NEVER_STARTED || screen == SceneScreen::QUIET_END;
}

//  Whether this state's step presses (DIALOG the config's text button,
//  BLIND_PRESS its blind button, MISSION_COMPLETE A); the others wait.
bool scene_screen_presses(SceneScreen screen){
    return screen == SceneScreen::DIALOG || screen == SceneScreen::MISSION_COMPLETE || screen == SceneScreen::BLIND_PRESS;
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
    if (!m_config.blind_press_after || now - m_last_cue < *m_config.blind_press_after){
        return false;
    }
    return !m_last_blind_press || now - *m_last_blind_press >= BLIND_PRESS_EVERY;
}

SceneScreen SceneTimeline::classify(const ScreenObservation& look) const{
    if (m_config.stop_at_battle && look.events.has(ScreenEvent::BATTLE_STARTED)){
        return SceneScreen::BATTLE;
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
    return classify_open(look);
}

SceneScreen SceneTimeline::classify_open(const ScreenObservation& look) const{
    if (look.readings.contains(MISSION_COMPLETE_READING)){
        return SceneScreen::MISSION_COMPLETE;
    }
    if (look.readings.contains(MISSION_BANNER_READING)){
        return m_banner_looks >= 1 ? SceneScreen::MISSION_BANNER : SceneScreen::BANNER_SHOWING;
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
    if (m_config.start_window && !m_scene_seen && look.when - m_start >= *m_config.start_window){
        return SceneScreen::NEVER_STARTED;
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
    if (classify(look) == SceneScreen::BLIND_PRESS){
        m_last_blind_press = look.when;
    }
    if (busy(look)){
        m_last_busy = look.when;
    }
    m_scene_seen = m_scene_seen || scene_shows(look);
    const bool box = look.events.has(ScreenEvent::DIALOG_BOX);
    const bool party_bar = party_bar_is_control(m_config) && look.events.has(ScreenEvent::PLAYER_CAN_MOVE);
    track_since(party_bar && !box, look.when, m_party_bar_since);
    track_since(look.readings.contains(CONTROLS_HINT_READING) && !box, look.when, m_hint_since);
    m_banner_looks = look.readings.contains(MISSION_BANNER_READING) && !box ? m_banner_looks + 1 : 0;
    if (busy(look) || m_party_bar_since || m_hint_since || m_banner_looks > 0){
        m_last_cue = look.when;
    }
}


//  "text box, advance arrow": a look's events and readings for the log.
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

//  What the states share: the backend, the timeline and the look they act on.
class SceneRunner{
public:
    SceneRunner(PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, WallClock start)
        : m_backend(backend)
        , m_label(label)
        , m_text_button(config.text_button)
        , m_blind_button(config.blind_button)
        , m_timeline(config, start)
    {}

    const SceneTimeline& timeline() const{
        return m_timeline;
    }

    //  The step of `screen`, the state of `look`.
    void act(SceneScreen screen, const ScreenObservation& look){
        m_timeline.record(look);
        const std::string events = describe_look(look);
        if (screen != m_logged_screen || events != m_logged_events){
            m_logged_screen = screen;
            m_logged_events = events;
            m_backend.log(std::format("[PLZA {}] {}: {}", m_label, scene_screen_name(screen), events));
        }
        if (scene_screen_terminal(screen)){
            return;
        }
        if (scene_screen_presses(screen)){
            m_backend.press(button_of(screen), /*hold=*/160ms, /*release=*/240ms);
            return;
        }
        wait(screen);
    }

private:
    //  The button `screen`'s step presses.
    Button button_of(SceneScreen screen) const{
        switch (screen){
        case SceneScreen::DIALOG:
            return m_text_button;
        case SceneScreen::BLIND_PRESS:
            return m_blind_button;
        default:
            return BUTTON_A;
        }
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
            wait_until(STILL_WAIT, [this](const ScreenObservation& look){
                const std::optional<WallClock> due = m_timeline.quiet_end_due();
                return (due && look.when >= *due) || m_timeline.blind_press_due(look.when) ||
                    !look.events.has(ScreenEvent::SCREEN_STILL);
            });
            return;
        case SceneScreen::PARTY_BAR:
        case SceneScreen::HINT_SHOWING:
        case SceneScreen::BANNER_SHOWING:
            m_backend.wait(POLL);
            return;
        default:
            m_backend.wait(2 * POLL);
            return;
        }
    }

    //  Looks every POLL until `done` or `limit`.
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
    std::string m_label;
    Button m_text_button;
    Button m_blind_button;
    SceneTimeline m_timeline;
    std::optional<SceneScreen> m_logged_screen;
    std::string m_logged_events;
};

//  How often a state may come up before the machine fails: a box or a
//  card that A does not get past.
size_t max_visits(SceneScreen screen){
    switch (screen){
    case SceneScreen::DIALOG:
        return 400;
    case SceneScreen::MISSION_COMPLETE:
        return 30;
    default:
        return SIZE_MAX;
    }
}

//  How long a state may see an unchanged screen: the waits (a fade, a
//  still cutscene, the hint or banner before it counts) for as long as
//  they wait. Zero for DIALOG, whose presses must change the screen.
Milliseconds max_still(SceneScreen screen){
    switch (screen){
    case SceneScreen::FADE_BLACK:
    case SceneScreen::FADE_WHITE:
        return FADE_WAIT;
    case SceneScreen::SCENE_STILL:
    case SceneScreen::BLIND_PRESS:
        //  A blind A press need not change the screen.
        return SCENE_STILL_WAIT;
    case SceneScreen::PARTY_BAR:
    case SceneScreen::HINT_SHOWING:
    case SceneScreen::BANNER_SHOWING:
        return STILL_WAIT;
    default:
        return Milliseconds::zero();
    }
}

//  Whether a new look shows the mission banner.
bool banner_shows(PlzaBackend& backend){
    return backend.observe().readings.contains(MISSION_BANNER_READING);
}

//  After the scene ended on the mission banner: looks until it has gone,
//  so the next step starts with control, for at most BANNER_GONE_WAIT;
//  then A up to BANNER_PRESSES times, BANNER_PRESS_EVERY apart, until it
//  goes.
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

//  The step's outcome from where the machine ended.
Pokemon::Outcome scene_outcome(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, const ScreenMachineResult& result
){
    if (!result.succeeded()){
        const std::string_view kind = result.failure.starts_with("Did not finish") ? "scene_timeout" : "scene_stuck";
        return Pokemon::outcome_failed(std::string(kind), std::format("{}: {}", label, result.failure));
    }
    if (result.terminal == scene_screen_name(SceneScreen::NEVER_STARTED)){
        return Pokemon::outcome_failed("scene_never_started", std::format(
            "{}: the scene never started (player not at its trigger?): no text, choice or battle in {} ms.",
            label, config.start_window ? config.start_window->count() : 0
        ));
    }
    if (result.terminal == scene_screen_name(SceneScreen::QUIET_END) && config.quiet_is_control){
        backend.log(std::format("[PLZA {}] done: quiet, taken as control.", label));
        return Pokemon::outcome_ok();
    }
    if (result.terminal == scene_screen_name(SceneScreen::QUIET_END)){
        //  Nothing busy for the quiet time, and no party bar, controls hint
        //  or banner:
        //  a cutscene with no text, or control the detectors miss.
        return Pokemon::outcome_failed("scene_quiet", std::format(
            "{}: nothing busy for {} ms and no sign of control.", label, config.quiet ? config.quiet->count() : 0
        ));
    }
    if (result.terminal == scene_screen_name(SceneScreen::MISSION_BANNER)){
        wait_banner_gone(backend, label);
    }
    backend.log(std::format("[PLZA {}] done on {}.", label, result.terminal));
    return Pokemon::outcome_ok();
}

} // namespace


Pokemon::Outcome run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout
){
    //  The look the states match and act on.
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
    StepTrace trace(std::format("PLZA {}", label), [&backend](const std::string& line){
        backend.log(line);
    });
    const ScreenMachineResult result = run_screen_machine(observe, backend.scope(), states, timeout, trace);
    return scene_outcome(backend, label, config, result);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
