/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <stdint.h>
#include <vector>
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/ScreenMachine/ScreenMachine.h"
#include "CommonTools/ScreenMachine/StepTrace.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonLZA_AutoStorySceneMachine.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::ScreenEvent;
using Pokemon::ScreenEvents;


namespace{

//  A party bar showing this long is the overworld: it also shows between
//  lines of a talk.
constexpr Milliseconds PARTY_BAR_HOLD = 1000ms;
//  Below this pixel RMSD two reads are the same screen (ScreenMachineChecks'
//  "unchanged").
constexpr double STILL_RMSD = 2.0;
//  The longest a fade is waited out in one step (02.01 holds a ~25 s black).
constexpr Milliseconds FADE_WAIT = 30000ms;
//  The longest a still screen is waited on in one step. ScreenMachineChecks
//  fails after 8 steps that change nothing, so a still frame may last 80 s.
constexpr Milliseconds STILL_WAIT = 10000ms;

const ScreenEvents& busy_events(){
    static const ScreenEvents busy{
        ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX, ScreenEvent::BATTLE_STARTED,
        ScreenEvent::FADE_BLACK, ScreenEvent::FADE_WHITE,
    };
    return busy;
}

constexpr SceneScreen ALL_SCREENS[] = {
    SceneScreen::GOAL_CHOICE, SceneScreen::GOAL_BATTLE,
    SceneScreen::FADE_BLACK, SceneScreen::FADE_WHITE,
    SceneScreen::CHOICE, SceneScreen::DIALOG,
    SceneScreen::CAN_MOVE, SceneScreen::PARTY_BAR, SceneScreen::QUIET_END,
    SceneScreen::SCENE_MOVING, SceneScreen::SCENE_STILL,
};

bool same_size(const ImageViewRGB32& a, const ImageViewRGB32& b){
    return a.width() == b.width() && a.height() == b.height();
}

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

SceneScreen SceneTimeline::classify(const ScreenEvents& events, bool still, WallClock now) const{
    if (m_config.goal == SceneMashGoal::CHOICE && events.has(ScreenEvent::CHOICE_BOX)){
        return SceneScreen::GOAL_CHOICE;
    }
    if (m_config.goal == SceneMashGoal::BATTLE && events.has(ScreenEvent::BATTLE_STARTED)){
        return SceneScreen::GOAL_BATTLE;
    }
    if (events.has(ScreenEvent::FADE_BLACK)){
        return SceneScreen::FADE_BLACK;
    }
    if (events.has(ScreenEvent::FADE_WHITE)){
        return SceneScreen::FADE_WHITE;
    }
    if (events.has(ScreenEvent::CHOICE_BOX)){
        return SceneScreen::CHOICE;
    }
    if (events.has(ScreenEvent::DIALOG_BOX)){
        return SceneScreen::DIALOG;
    }
    if (m_config.goal == SceneMashGoal::OVERWORLD && events.has(ScreenEvent::PLAYER_CAN_MOVE)){
        if (m_party_bar_since && now - *m_party_bar_since >= PARTY_BAR_HOLD){
            return SceneScreen::CAN_MOVE;
        }
        return SceneScreen::PARTY_BAR;
    }
    const std::optional<WallClock> due = quiet_end_due();
    if (due && now >= *due && !events.any_of(busy_events())){
        return SceneScreen::QUIET_END;
    }
    return still ? SceneScreen::SCENE_STILL : SceneScreen::SCENE_MOVING;
}

void SceneTimeline::record(const ScreenEvents& events, WallClock now){
    if (events.any_of(busy_events())){
        m_last_busy = now;
    }
    const bool party_bar = events.has(ScreenEvent::PLAYER_CAN_MOVE) &&
        !events.has(ScreenEvent::DIALOG_BOX) && !events.has(ScreenEvent::CHOICE_BOX);
    if (!party_bar){
        m_party_bar_since.reset();
    }else if (!m_party_bar_since){
        m_party_bar_since = now;
    }
}


namespace{

//  What the states share: the observer, the timeline, and the last frame
//  read, so the states of one frame read its events once.
class SceneReader{
public:
    SceneReader(ConsoleHandle& console, ProControllerContext& context, StepTrace& trace, const SceneMachineConfig& config)
        : m_console(console)
        , m_context(context)
        , m_trace(trace)
        , m_timeline(config, current_time())
    {}

    //  The state of `screen`; a frame already read gives its first reading.
    SceneScreen read(const ImageViewRGB32& screen){
        if (screen.data() == m_read_data && screen.width() == m_read_width && screen.height() == m_read_height){
            return m_read_screen;
        }
        m_read_data = screen.data();
        m_read_width = screen.width();
        m_read_height = screen.height();
        m_read_events = m_observer.read(screen);
        const bool still = m_previous && same_size(*m_previous.frame, screen) &&
            ImageMatch::pixel_RMSD(*m_previous.frame, screen) < STILL_RMSD;
        m_read_screen = m_timeline.classify(m_read_events, still, current_time());
        return m_read_screen;
    }

    //  The step of `screen`, the state the machine read last.
    void act(SceneScreen screen){
        const WallClock now = current_time();
        m_timeline.record(m_read_events, now);
        if (m_read_events != m_logged_events){
            m_logged_events = m_read_events;
            m_trace.action(std::format("events: {}", Pokemon::describe(m_read_events)));
        }
        m_previous = m_console.video().snapshot();
        m_read_data = nullptr;
        switch (scene_screen_action(screen, m_timeline.config())){
        case SceneMashAction::DONE:
            return;
        case SceneMashAction::PRESS_TEXT:
            pbf_press_button(m_context, m_timeline.config().text_button, 160ms, 240ms);
            return;
        case SceneMashAction::PRESS_A:
            pbf_press_button(m_context, BUTTON_A, 160ms, 240ms);
            return;
        case SceneMashAction::PRESS_B:
            pbf_press_button(m_context, BUTTON_B, 160ms, 240ms);
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
            wait_until(FADE_WAIT, [](const ImageViewRGB32& frame){
                return Pokemon::fade_of_screen(frame) == Pokemon::WarpFade::NONE;
            });
            return;
        case SceneScreen::SCENE_STILL:
            wait_until(STILL_WAIT, [this](const ImageViewRGB32& frame){
                const std::optional<WallClock> due = m_timeline.quiet_end_due();
                return (due && current_time() >= *due) || !m_previous ||
                    !same_size(*m_previous.frame, frame) ||
                    ImageMatch::pixel_RMSD(*m_previous.frame, frame) >= STILL_RMSD;
            });
            return;
        case SceneScreen::PARTY_BAR:
            m_context.wait_for(250ms);
            return;
        default:
            m_context.wait_for(500ms);
            return;
        }
    }

    //  Reads a frame every 250 ms until `done` or `limit`.
    template <typename Done>
    void wait_until(Milliseconds limit, Done&& done){
        const WallClock end = current_time() + limit;
        while (current_time() < end){
            m_context.wait_for(250ms);
            const VideoSnapshot frame = m_console.video().snapshot();
            if (frame && done(*frame.frame)){
                return;
            }
        }
    }

    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    StepTrace& m_trace;
    PlzaScreenObserver m_observer;
    SceneTimeline m_timeline;
    VideoSnapshot m_previous;
    const uint32_t* m_read_data = nullptr;
    size_t m_read_width = 0;
    size_t m_read_height = 0;
    ScreenEvents m_read_events;
    ScreenEvents m_logged_events;
    SceneScreen m_read_screen = SceneScreen::SCENE_MOVING;
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


void run_scene_machine(
    ConsoleHandle& console, ProControllerContext& context,
    std::string_view label, const SceneMachineConfig& config, Milliseconds timeout
){
    StepTrace trace(console, std::format("PLZA {}", label));
    SceneReader reader(console, context, trace, config);
    std::vector<ScreenState> states;
    for (const SceneScreen screen : ALL_SCREENS){
        ScreenState state;
        state.name = std::string(scene_screen_name(screen));
        state.matches = [&reader, screen](const ImageViewRGB32& frame){
            return reader.read(frame) == screen;
        };
        state.act = [&reader, screen]{
            reader.act(screen);
        };
        state.max_visits = max_visits(screen);
        state.terminal = scene_screen_terminal(screen);
        states.push_back(std::move(state));
    }
    const std::string end = run_screen_machine(console, context, states, timeout, trace);
    console.log(std::format("[PLZA {}] done on {}.", label, end));
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
