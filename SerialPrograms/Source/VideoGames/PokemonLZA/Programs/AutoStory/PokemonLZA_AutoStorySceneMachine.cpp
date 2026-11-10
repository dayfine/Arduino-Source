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
//  The longest a fade is waited out in one step (02.01 holds a ~25 s black).
constexpr Milliseconds FADE_WAIT = 30000ms;
//  The longest a still screen is waited on in one step.
constexpr Milliseconds STILL_WAIT = 10000ms;
//  How often a wait looks again.
constexpr Milliseconds POLL = 250ms;
//  Looks a machine takes at most, besides its timeout: a backend whose
//  looks carry no time still ends.
constexpr size_t MAX_LOOKS = 4000;

//  Something holds the player: a box (or choice), a battle, a fade.
bool busy(const ScreenObservation& look){
    return look.events.has(EngineEvent::TEXT_BOX) || look.events.has(EngineEvent::BATTLE) ||
        look.events.has(EngineEvent::FADE_BLACK) || look.events.has(EngineEvent::FADE_WHITE) ||
        has_choice(look);
}

constexpr SceneScreen ALL_SCREENS[] = {
    SceneScreen::GOAL_CHOICE, SceneScreen::GOAL_BATTLE,
    SceneScreen::FADE_BLACK, SceneScreen::FADE_WHITE,
    SceneScreen::CHOICE, SceneScreen::DIALOG,
    SceneScreen::CAN_MOVE, SceneScreen::PARTY_BAR, SceneScreen::QUIET_END,
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
    case SceneScreen::CAN_MOVE:
        return "can_move";
    case SceneScreen::PARTY_BAR:
        return "party_bar";
    case SceneScreen::QUIET_END:
        return "quiet_end";
    case SceneScreen::SCENE_MOVING:
        return "scene_moving";
    case SceneScreen::SCENE_STILL:
        return "scene_still";
    }
    return "unknown";
}

bool scene_screen_terminal(SceneScreen screen){
    return screen == SceneScreen::GOAL_CHOICE || screen == SceneScreen::GOAL_BATTLE ||
        screen == SceneScreen::CAN_MOVE || screen == SceneScreen::QUIET_END;
}

SceneMashAction scene_screen_action(SceneScreen screen, const SceneMachineConfig& config){
    if (scene_screen_terminal(screen)){
        return SceneMashAction::DONE;
    }
    switch (screen){
    case SceneScreen::CHOICE:
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
{}

std::optional<WallClock> SceneTimeline::quiet_end_due() const{
    if (m_config.goal != SceneMashGoal::OVERWORLD || !m_config.quiet){
        return std::nullopt;
    }
    return std::max(m_quiet_from, m_last_busy + *m_config.quiet);
}

SceneScreen SceneTimeline::classify(const ScreenObservation& look) const{
    const bool choice = has_choice(look);
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
    if (look.events.has(EngineEvent::TEXT_BOX)){
        return SceneScreen::DIALOG;
    }
    if (m_config.goal == SceneMashGoal::OVERWORLD && look.events.has(EngineEvent::CAN_MOVE)){
        if (m_party_bar_since && look.when - *m_party_bar_since >= PARTY_BAR_HOLD){
            return SceneScreen::CAN_MOVE;
        }
        return SceneScreen::PARTY_BAR;
    }
    const std::optional<WallClock> due = quiet_end_due();
    if (due && look.when >= *due && !busy(look)){
        return SceneScreen::QUIET_END;
    }
    return look.events.has(EngineEvent::STILL) ? SceneScreen::SCENE_STILL : SceneScreen::SCENE_MOVING;
}

void SceneTimeline::record(const ScreenObservation& look){
    if (busy(look)){
        m_last_busy = look.when;
    }
    const bool party_bar = look.events.has(EngineEvent::CAN_MOVE) && !look.events.has(EngineEvent::TEXT_BOX) && !has_choice(look);
    if (!party_bar){
        m_party_bar_since.reset();
    }else if (!m_party_bar_since){
        m_party_bar_since = look.when;
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
            text += (text.empty() ? "" : ", ") + std::string(name);
        }
    }
    if (has_choice(look)){
        text += text.empty() ? "choice" : ", choice";
    }
    return text.empty() ? "nothing" : text;
}

//  What the states share: the backend, the timeline and the look they act on.
class SceneRunner{
public:
    SceneRunner(PlzaStoryBackend& backend, std::string_view label, const SceneMachineConfig& config, WallClock start)
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
                return (due && look.when >= *due) || !look.events.has(EngineEvent::STILL);
            });
            return;
        case SceneScreen::PARTY_BAR:
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

    PlzaStoryBackend& m_backend;
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
        return 30;
    default:
        return SIZE_MAX;
    }
}

} // namespace


Pokemon::Outcome run_scene_machine(
    PlzaStoryBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout
){
    Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
    const WallClock start = look.when;
    SceneRunner runner(backend, label, config, start);
    std::vector<ScreenState> states;
    std::map<std::string, SceneScreen, std::less<>> screens;
    for (const SceneScreen screen : ALL_SCREENS){
        ScreenState state;
        state.name = std::string(scene_screen_name(screen));
        state.matches_observation = [&runner, screen](const ScreenObservation& observation){
            return runner.timeline().classify(observation) == screen;
        };
        state.max_visits = max_visits(screen);
        state.terminal = scene_screen_terminal(screen);
        screens.emplace(state.name, screen);
        states.push_back(std::move(state));
    }
    std::map<std::string, size_t, std::less<>> visits;
    for (size_t looks = 0; looks < MAX_LOOKS; looks++){
        if (looks > 0){
            look = backend.observe();
        }
        if (look.when - start > timeout){
            break;
        }
        const ScreenState* state = match_screen_state(states, look);
        if (state == nullptr){
            return Pokemon::outcome_failed("scene_unknown", std::format("{}: no state for {}", label, describe_look(look)));
        }
        if (++visits[state->name] > state->max_visits){
            return Pokemon::outcome_failed("scene_stuck", std::format("{}: {} came up {} times", label, state->name, state->max_visits + 1));
        }
        runner.act(screens.at(state->name), look);
        if (state->terminal){
            backend.log(std::format("[PLZA {}] done on {}.", label, state->name));
            return Pokemon::outcome_ok();
        }
    }
    return Pokemon::outcome_failed("scene_timeout", std::format("{}: not done within {} s", label, timeout.count() / 1000));
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
