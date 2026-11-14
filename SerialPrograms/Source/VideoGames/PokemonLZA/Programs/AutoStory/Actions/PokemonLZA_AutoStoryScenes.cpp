/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  When an OVERWORLD mash is over: the party bar held a second, or (with a
//  quiet time) nothing busy on screen for that long once the scene's
//  minimum length has passed.
class OverworldEnd{
public:
    OverworldEnd(std::optional<Milliseconds> quiet, Milliseconds min_length)
        : m_quiet(quiet)
        , m_quiet_from(current_time() + min_length)
        , m_last_busy(current_time())
    {}
    bool reached(ConsoleHandle& console, const ScreenEvents& events, SceneMashAction action, WallClock now){
        if (events.any_of({
            ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX,
            ScreenEvent::BATTLE_STARTED, ScreenEvent::FADE_BLACK,
            ScreenEvent::FADE_WHITE,
        })){
            m_last_busy = now;
        }
        if (action != SceneMashAction::DONE){
            m_can_move_since.reset();
        }else if (!m_can_move_since){
            m_can_move_since = now;
        }
        if (m_can_move_since && now - *m_can_move_since >= 1000ms){
            return true;
        }
        if (m_quiet && now >= m_quiet_from && now - m_last_busy >= *m_quiet){
            console.log(std::format("mash_scene(): quiet for {} ms, taking it as the overworld.", m_quiet->count()));
            return true;
        }
        return false;
    }

private:
    std::optional<Milliseconds> m_quiet;
    WallClock m_quiet_from;
    WallClock m_last_busy;
    std::optional<WallClock> m_can_move_since;
};

void press_for(ProControllerContext& context, SceneMashAction action, Button text_button){
    switch (action){
    case SceneMashAction::PRESS_TEXT:
        pbf_press_button(context, text_button, 160ms, 240ms);
        return;
    case SceneMashAction::PRESS_A:
        pbf_press_button(context, BUTTON_A, 160ms, 240ms);
        return;
    case SceneMashAction::PRESS_B:
        pbf_press_button(context, BUTTON_B, 160ms, 240ms);
        return;
    case SceneMashAction::WAIT:
    case SceneMashAction::DONE:
        context.wait_for(250ms);
        return;
    }
}

//  Logs the frame's events whenever they change (the still-screen flag left
//  out), so a run's log shows each scene's fades, boxes and party bar.
void log_event_change(ConsoleHandle& console, const ScreenEvents& events, ScreenEvents& last){
    ScreenEvents shown = events;
    shown.set(ScreenEvent::SCREEN_STILL, false);
    if (shown == last){
        return;
    }
    last = shown;
    console.log(std::format("mash_scene(): screen now: {}", describe(shown)));
}

} // namespace

namespace{

//  When a scene mash ends besides its goal: mash_scene()'s quiet end, and a
//  start window (SceneStart).
struct SceneMashLimits{
    std::optional<Milliseconds> quiet = std::nullopt;
    Milliseconds min_length = Milliseconds(0);
    std::optional<Milliseconds> start_window = std::nullopt;
};

void mash_scene_until(
    ConsoleHandle& console, ProControllerContext& context,
    SceneMashGoal goal, Button text_button, Milliseconds timeout, const SceneMashLimits& limits
){
    PlzaScreenObserver observer;
    OverworldEnd overworld(limits.quiet, limits.min_length);
    ScreenEvents last_events;
    const WallClock start = current_time();
    SceneStart scene(start, limits.start_window.value_or(timeout));
    while (current_time() - start < timeout){
        context.wait_for_all_requests();
        const WallClock now = current_time();
        const VideoSnapshot frame = console.video().snapshot();
        if (frame && tutorial_card_shows(frame)){
            console.log("mash_scene(): a tutorial card: A.");
            scene.see();
            pbf_press_button(context, BUTTON_A, 160ms, 840ms);
            continue;
        }
        const ScreenEvents events = observer.observe(frame, now);
        log_event_change(console, events, last_events);
        if (limits.start_window && scene.never_started(events, now)){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::format(
                    "mash_scene(): the scene never started (player not at its trigger?): no box, choice or battle in {} ms.",
                    limits.start_window->count()
                ),
                console
            );
        }
        const SceneMashAction action = scene_mash_action(events, goal, text_button == BUTTON_B);
        if (goal == SceneMashGoal::OVERWORLD){
            if (overworld.reached(console, events, action, now)){
                return;
            }
        }else if (action == SceneMashAction::DONE){
            return;
        }
        press_for(context, action, text_button);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("mash_scene(): the goal did not show within {} ms.", timeout.count()),
        console
    );
}

} // namespace

void mash_scene(
    ConsoleHandle& console, ProControllerContext& context,
    SceneMashGoal goal, Button text_button, Milliseconds timeout,
    std::optional<Milliseconds> quiet, Milliseconds min_length
){
    mash_scene_until(console, context, goal, text_button, timeout, SceneMashLimits{.quiet = quiet, .min_length = min_length});
}

void mash_story_scene(ConsoleHandle& console, ProControllerContext& context, Button text_button){
    mash_scene_until(
        console, context, SceneMashGoal::OVERWORLD, text_button, Milliseconds(120000),
        SceneMashLimits{.start_window = SCENE_START_WINDOW}
    );
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
