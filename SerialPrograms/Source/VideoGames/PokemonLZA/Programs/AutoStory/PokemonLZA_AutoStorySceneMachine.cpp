/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <stdint.h>
#include <vector>
#include "CommonTools/ScreenMachine/ScreenMachine.h"
#include "PokemonLZA_AutoStorySceneMachine.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using EngineEvent = PokemonAutomation::ScreenEvent;


namespace{

//  A party bar showing this long is the overworld: it also shows between
//  lines of a talk.
constexpr Milliseconds PARTY_BAR_HOLD = 1000ms;
//  The controls hint showing this long is free control: two looks or more.
constexpr Milliseconds CONTROLS_HINT_HOLD = 1000ms;
//  The longest a fade is waited out in one step (02.04 holds a ~25 s black).
constexpr Milliseconds FADE_WAIT = 30000ms;
//  The longest a still screen is waited on in one step.
constexpr Milliseconds STILL_WAIT = 10000ms;
//  The longest a cutscene may hold one still frame.
constexpr Milliseconds SCENE_STILL_WAIT = 60000ms;
//  How often blind_press_after presses A.
constexpr Milliseconds BLIND_PRESS_EVERY = 2000ms;
//  How often a wait looks again.
constexpr Milliseconds POLL = 250ms;
//  The longest the machine waits for the mission banner to go once it has
//  ended the scene (it shows ~3 s).
constexpr Milliseconds BANNER_GONE_WAIT = 10000ms;
//  A banner still up after BANNER_GONE_WAIT gets A this many times, this
//  far apart, until it goes: 01.03's "MAIN MISSION 02" stayed past 10 s
//  (console B, 2026-11-10), where "MAIN MISSION 01" went in ~3 s.
constexpr size_t BANNER_PRESSES = 3;
constexpr Milliseconds BANNER_PRESS_EVERY = 1000ms;

//  Text to press on: a box (or choice), or the advance chevron without one.
bool text_shows(const ScreenObservation& look){
    return look.events.has(EngineEvent::TEXT_BOX) || look.events.has(EngineEvent::CHOICE_BOX) ||
        look.readings.contains(ADVANCE_ARROW_READING);
}

//  Something holds the player: text, a battle, a fade, the mission
//  complete card.
bool busy(const ScreenObservation& look){
    return text_shows(look) || look.events.has(EngineEvent::BATTLE) ||
        look.events.has(EngineEvent::FADE_BLACK) || look.events.has(EngineEvent::FADE_WHITE) ||
        look.readings.contains(MISSION_COMPLETE_READING);
}

//  The mission banner, and not the complete card (which has its bar).
bool banner_shows(const ScreenObservation& look){
    return look.readings.contains(MISSION_BANNER_READING) && !look.readings.contains(MISSION_COMPLETE_READING);
}

//  Free control on screen: the controls hint, or an "A: Talk" prompt
//  (01.02's battle area shows no controls hint, console B 2026-11-10).
bool control_shows(const ScreenObservation& look){
    return look.readings.contains(CONTROLS_HINT_READING) || look.events.has(EngineEvent::INTERACT_PROMPT);
}

constexpr SceneScreen ALL_SCREENS[] = {
    SceneScreen::GOAL_CHOICE, SceneScreen::GOAL_BATTLE,
    SceneScreen::FADE_BLACK, SceneScreen::FADE_WHITE,
    SceneScreen::CHOICE, SceneScreen::DIALOG, SceneScreen::MISSION_COMPLETE,
    SceneScreen::MISSION_BANNER, SceneScreen::BANNER_SHOWING,
    SceneScreen::CAN_MOVE, SceneScreen::PARTY_BAR,
    SceneScreen::CONTROLS_HINT, SceneScreen::HINT_SHOWING, SceneScreen::QUIET_END, SceneScreen::BLIND_PRESS,
    SceneScreen::SCENE_MOVING, SceneScreen::SCENE_STILL,
};

} // namespace


std::string_view scene_screen_name(SceneScreen screen){
    switch (screen){
    case SceneScreen::GOAL_CHOICE:
        return "goal_choice";
    case SceneScreen::GOAL_BATTLE:
        return "goal_battle";
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

bool scene_screen_terminal(SceneScreen screen){
    return screen == SceneScreen::GOAL_CHOICE || screen == SceneScreen::GOAL_BATTLE ||
        screen == SceneScreen::MISSION_BANNER || screen == SceneScreen::CAN_MOVE ||
        screen == SceneScreen::CONTROLS_HINT || screen == SceneScreen::QUIET_END;
}

SceneMashAction scene_screen_action(SceneScreen screen, const SceneMachineConfig& config){
    if (scene_screen_terminal(screen)){
        return SceneMashAction::DONE;
    }
    switch (screen){
    case SceneScreen::CHOICE:
    case SceneScreen::MISSION_COMPLETE:
    case SceneScreen::BLIND_PRESS:
        return SceneMashAction::PRESS_A;
    case SceneScreen::DIALOG:
        return SceneMashAction::PRESS_TEXT;
    case SceneScreen::SCENE_MOVING:
    case SceneScreen::SCENE_STILL:
        if (config.goal == SceneMashGoal::BATTLE){
            return SceneMashAction::PRESS_A;
        }
        return config.text_button == BUTTON_B ? SceneMashAction::PRESS_B : SceneMashAction::WAIT;
    default:
        return SceneMashAction::WAIT;
    }
}


SceneTimeline::SceneTimeline(SceneMachineConfig config, WallClock start)
    : m_config(std::move(config))
    , m_quiet_from(start + m_config.min_length)
    , m_last_busy(start)
    , m_last_cue(start)
{}

std::optional<WallClock> SceneTimeline::quiet_end_due() const{
    if (m_config.goal != SceneMashGoal::OVERWORLD || !m_config.quiet){
        return std::nullopt;
    }
    return std::max(m_quiet_from, m_last_busy + *m_config.quiet);
}

bool SceneTimeline::blind_press_due(WallClock now) const{
    if (!m_config.blind_press_after || m_config.goal != SceneMashGoal::OVERWORLD || m_config.text_button != BUTTON_A){
        return false;
    }
    if (now - m_last_cue < *m_config.blind_press_after){
        return false;
    }
    return !m_last_blind_press || now - *m_last_blind_press >= BLIND_PRESS_EVERY;
}

SceneScreen SceneTimeline::classify(const ScreenObservation& look) const{
    const bool choice = look.events.has(EngineEvent::CHOICE_BOX);
    if (m_config.goal == SceneMashGoal::CHOICE && choice){
        return SceneScreen::GOAL_CHOICE;
    }
    if (m_config.goal == SceneMashGoal::BATTLE && look.events.has(EngineEvent::BATTLE)){
        return SceneScreen::GOAL_BATTLE;
    }
    if (look.events.has(EngineEvent::FADE_BLACK)){
        return SceneScreen::FADE_BLACK;
    }
    if (look.events.has(EngineEvent::FADE_WHITE)){
        return SceneScreen::FADE_WHITE;
    }
    if (choice){
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
    if (m_config.goal == SceneMashGoal::OVERWORLD && banner_shows(look)){
        return m_banner_looks >= 1 ? SceneScreen::MISSION_BANNER : SceneScreen::BANNER_SHOWING;
    }
    if (m_config.goal == SceneMashGoal::OVERWORLD && look.events.has(EngineEvent::CAN_MOVE)){
        if (m_party_bar_since && look.when - *m_party_bar_since >= PARTY_BAR_HOLD){
            return SceneScreen::CAN_MOVE;
        }
        return SceneScreen::PARTY_BAR;
    }
    if (m_config.goal == SceneMashGoal::OVERWORLD && control_shows(look)){
        if (m_hint_since && look.when - *m_hint_since >= CONTROLS_HINT_HOLD){
            return SceneScreen::CONTROLS_HINT;
        }
        return SceneScreen::HINT_SHOWING;
    }
    const std::optional<WallClock> due = quiet_end_due();
    if (due && look.when >= *due && !busy(look)){
        return SceneScreen::QUIET_END;
    }
    //  Not at an interact prompt: A would talk to whoever it names.
    if (blind_press_due(look.when) && !look.events.has(EngineEvent::INTERACT_PROMPT)){
        return SceneScreen::BLIND_PRESS;
    }
    return look.events.has(EngineEvent::STILL) ? SceneScreen::SCENE_STILL : SceneScreen::SCENE_MOVING;
}

namespace{

//  Starts `since` when `shows` starts, clears it when it stops.
void track_since(bool shows, WallClock when, std::optional<WallClock>& since){
    if (!shows){
        since.reset();
    }else if (!since){
        since = when;
    }
}

} // namespace

void SceneTimeline::record(const ScreenObservation& look){
    if (classify(look) == SceneScreen::BLIND_PRESS){
        m_last_blind_press = look.when;
    }
    if (busy(look)){
        m_last_busy = look.when;
    }
    const bool box = look.events.has(EngineEvent::TEXT_BOX) || look.events.has(EngineEvent::CHOICE_BOX);
    track_since(look.events.has(EngineEvent::CAN_MOVE) && !box, look.when, m_party_bar_since);
    track_since(control_shows(look) && !box, look.when, m_hint_since);
    m_banner_looks = banner_shows(look) && !box ? m_banner_looks + 1 : 0;
    if (busy(look) || m_party_bar_since || m_hint_since || m_banner_looks > 0){
        m_last_cue = look.when;
    }
}


namespace{

//  "text box, choice": a look's events for the log.
std::string describe_look(const ScreenObservation& look){
    const std::pair<EngineEvent, std::string_view> names[] = {
        {EngineEvent::FADE_BLACK, "black fade"}, {EngineEvent::FADE_WHITE, "white fade"},
        {EngineEvent::TEXT_BOX, "text box"}, {EngineEvent::BATTLE, "battle"},
        {EngineEvent::CAN_MOVE, "party bar"}, {EngineEvent::STILL, "still"},
    };
    std::string text;
    for (const auto& [event, name] : names){
        if (look.events.has(event)){
            text += std::format("{}{}", text.empty() ? "" : ", ", name);
        }
    }
    if (look.events.has(EngineEvent::CHOICE_BOX)){
        text += text.empty() ? "choice" : ", choice";
    }
    if (look.readings.contains(CONTROLS_HINT_READING)){
        text += text.empty() ? "controls hint" : ", controls hint";
    }
    if (look.readings.contains(ADVANCE_ARROW_READING)){
        text += text.empty() ? "advance arrow" : ", advance arrow";
    }
    if (look.readings.contains(MISSION_BANNER_READING)){
        text += text.empty() ? "mission banner" : ", mission banner";
    }
    if (look.readings.contains(MISSION_COMPLETE_READING)){
        text += text.empty() ? "mission complete" : ", mission complete";
    }
    return text.empty() ? "nothing" : text;
}

//  What the states share: the backend, the timeline and the look they act on.
class SceneRunner{
public:
    SceneRunner(PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, WallClock start)
        : m_backend(backend)
        , m_label(label)
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
        switch (scene_screen_action(screen, m_timeline.config())){
        case SceneMashAction::DONE:
            return;
        case SceneMashAction::PRESS_TEXT:
            m_backend.press(m_timeline.config().text_button, 160ms, 240ms);
            return;
        case SceneMashAction::PRESS_A:
            m_backend.press(BUTTON_A, 160ms, 240ms);
            return;
        case SceneMashAction::PRESS_B:
            m_backend.press(BUTTON_B, 160ms, 240ms);
            return;
        case SceneMashAction::WAIT:
            wait(screen);
            return;
        }
    }

private:
    void wait(SceneScreen screen){
        switch (screen){
        case SceneScreen::FADE_BLACK:
        case SceneScreen::FADE_WHITE:
            wait_until(FADE_WAIT, [](const ScreenObservation& look){
                return !look.events.has(EngineEvent::FADE_BLACK) && !look.events.has(EngineEvent::FADE_WHITE);
            });
            return;
        case SceneScreen::SCENE_STILL:
            wait_until(STILL_WAIT, [this](const ScreenObservation& look){
                const std::optional<WallClock> due = m_timeline.quiet_end_due();
                return (due && look.when >= *due) || m_timeline.blind_press_due(look.when) ||
                    !look.events.has(EngineEvent::STILL);
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
    SceneTimeline m_timeline;
    std::optional<SceneScreen> m_logged_screen;
    std::string m_logged_events;
};

//  How often a state may come up before the machine fails: a box or a
//  choice that a press does not get past.
size_t max_visits(SceneScreen screen){
    switch (screen){
    case SceneScreen::DIALOG:
        return 400;
    case SceneScreen::CHOICE:
    case SceneScreen::MISSION_COMPLETE:
        return 30;
    default:
        return SIZE_MAX;
    }
}

//  How long a state may see an unchanged screen: the waits (a fade, a
//  still cutscene, the party bar held before it counts) for as long as
//  they wait. Zero for the rest, whose presses must change the screen.
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

} // namespace


namespace{

//  run_screen_machine_on_observations()'s failure as an Outcome.
Pokemon::Outcome scene_failure(std::string_view label, const std::string& failure){
    std::string_view kind = "scene_stuck";
    if (failure.starts_with("Did not finish")){
        kind = "scene_timeout";
    }else if (failure.starts_with("The screen matches none")){
        kind = "scene_unknown";
    }
    return Pokemon::outcome_failed(std::string(kind), std::format("{}: {}", label, failure));
}

//  Whether a new look shows the mission banner.
bool banner_still_shows(PlzaBackend& backend){
    return backend.observe().readings.contains(MISSION_BANNER_READING);
}

//  After the scene ended on the mission banner: looks until it has gone,
//  so the next step starts with control, for at most BANNER_GONE_WAIT.
//  Whether it went.
bool wait_banner_gone(PlzaBackend& backend, std::string_view label){
    for (Milliseconds waited = 0ms; waited < BANNER_GONE_WAIT; waited += POLL){
        if (!banner_still_shows(backend)){
            backend.log(std::format("[PLZA {}] the mission banner has gone after {} ms.", label, waited.count()));
            return true;
        }
        backend.wait(POLL);
    }
    return false;
}

//  A banner that stayed past BANNER_GONE_WAIT: A up to BANNER_PRESSES
//  times, BANNER_PRESS_EVERY apart, until it goes.
void press_banner_away(PlzaBackend& backend, std::string_view label){
    backend.log(std::format("[PLZA {}] the mission banner still shows after {} ms; pressing A.", label, BANNER_GONE_WAIT.count()));
    for (size_t presses = 1; presses <= BANNER_PRESSES; presses++){
        backend.press(BUTTON_A, 160ms, 240ms);
        backend.wait(BANNER_PRESS_EVERY);
        if (!banner_still_shows(backend)){
            backend.log(std::format("[PLZA {}] the mission banner has gone after {} A presses.", label, presses));
            return;
        }
    }
    backend.log(std::format("[PLZA {}] the mission banner still shows after {} A presses; going on.", label, BANNER_PRESSES));
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
        state.matches_observation = [&runner, screen](const ScreenObservation& observation){
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
    const ScreenMachineResult result = run_screen_machine_on_observations(observe, backend.scope(), states, timeout, trace);
    if (!result.succeeded()){
        return scene_failure(label, result.failure);
    }
    if (result.terminal == scene_screen_name(SceneScreen::QUIET_END) && config.quiet_is_control){
        backend.log(std::format("[PLZA {}] done: quiet, taken as control.", label));
        return Pokemon::outcome_ok();
    }
    if (result.terminal == scene_screen_name(SceneScreen::QUIET_END)){
        //  Nothing busy for the quiet time, and no party bar or controls
        //  hint: a cutscene with no text, or control the detectors miss.
        return Pokemon::outcome_failed("scene_quiet", std::format(
            "{}: nothing busy for {} ms and no sign of control.", label, config.quiet ? config.quiet->count() : 0
        ));
    }
    if (result.terminal == scene_screen_name(SceneScreen::MISSION_BANNER) && !wait_banner_gone(backend, label)){
        press_banner_away(backend, label);
    }
    backend.log(std::format("[PLZA {}] done on {}.", label, result.terminal));
    return Pokemon::outcome_ok();
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
