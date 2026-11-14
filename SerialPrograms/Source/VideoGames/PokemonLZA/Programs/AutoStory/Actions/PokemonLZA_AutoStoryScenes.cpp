/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryDialogs.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  How often a mash looks when it waits.
constexpr Milliseconds MASH_POLL = 250ms;

//  Logs the look's events whenever they change (the still-screen flag left
//  out), so a run's log shows each scene's fades, boxes and party bar.
void log_event_change(PlzaBackend& backend, const ScreenEvents& events, ScreenEvents& last){
    ScreenEvents shown = events;
    shown.set(ScreenEvent::SCREEN_STILL, false);
    if (shown == last){
        return;
    }
    last = shown;
    backend.log(std::format("mash_scene(): screen now: {}", describe(shown)));
}

void press_for(PlzaBackend& backend, SceneMashAction action, Button text_button){
    switch (action){
    case SceneMashAction::PRESS_TEXT:
        backend.press(text_button, 160ms, 240ms);
        return;
    case SceneMashAction::PRESS_A:
        backend.press(BUTTON_A, 160ms, 240ms);
        return;
    case SceneMashAction::PRESS_B:
        backend.press(BUTTON_B, 160ms, 240ms);
        return;
    case SceneMashAction::WAIT:
    case SceneMashAction::DONE:
        backend.wait(MASH_POLL);
        return;
    }
}

} // namespace

Pokemon::Outcome mash_scene(
    PlzaBackend& backend, SceneMashGoal goal, Button text_button, Milliseconds timeout,
    std::optional<Milliseconds> start_window
){
    SceneMash mash(goal, text_button);
    ScreenEvents last_events;
    //  Set by the first look: when the mash started, and its scene's start
    //  window from there.
    struct Started{
        WallClock at;
        SceneStart scene;
    };
    std::optional<Started> started;
    //  Every look presses or waits at least MASH_POLL on a console; a
    //  scripted backend's clock stands still between inputs.
    const size_t max_looks = static_cast<size_t>(timeout / MASH_POLL) + 1;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (!started){
            started = Started{look.when, SceneStart(look.when, start_window.value_or(timeout))};
        }
        if (look.when - started->at >= timeout){
            break;
        }
        if (look.readings.contains(TUTORIAL_CARD_READING)){
            backend.log("mash_scene(): a tutorial card: A.");
            started->scene.see();
            backend.press(BUTTON_A, 160ms, 840ms);
            continue;
        }
        log_event_change(backend, look.events, last_events);
        if (start_window && started->scene.never_started(look.events, look.when)){
            return Pokemon::outcome_failed("scene_never_started", std::format(
                "mash_scene(): the scene never started (player not at its trigger?): no box, choice or battle in {} ms.",
                start_window->count()
            ));
        }
        const SceneMashAction action = mash.look(look.events, look.when);
        if (action == SceneMashAction::DONE){
            return Pokemon::outcome_ok();
        }
        press_for(backend, action, text_button);
    }
    return Pokemon::outcome_failed("scene_timeout", std::format("mash_scene(): the goal did not show within {} ms.", timeout.count()));
}

Pokemon::Outcome mash_story_scene(PlzaBackend& backend, Button text_button){
    return mash_scene(backend, SceneMashGoal::OVERWORLD, text_button, Milliseconds(120000), SCENE_START_WINDOW);
}


//  Taunie's talk, the Map & Missions card, the first map and the rest of her
//  talk: A and B in turn, until the party bar has held (OVERWORLD_HOLD). A pages
//  the talk and the card; B closes the map, which takes no A. The map is not
//  waited for: MapDetector needs the Travel Spots icon, and on console B
//  (2026-11-13) it never fired on this first map in 60 s of A.
void talk_through_map_screen(ConsoleHandle& console, ProControllerContext& context){
    press_until_overworld(
        console, context,
        [](ProControllerContext& context){
            pbf_press_button(context, BUTTON_A, 160ms, 840ms);
            pbf_press_button(context, BUTTON_B, 160ms, 840ms);
        },
        120s, "talk_through_map_screen(): no overworld within 120 s of A and B in turn."
    );
    console.log("talk_through_map_screen(): the party bar held: back in the overworld.");
}

void climb_ladder(ConsoleHandle&, ProControllerContext& context){
    pbf_move_left_joystick(context, {1, 0}, 600ms, 300ms);
    pbf_move_left_joystick(context, {0, 1}, 150ms, 300ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
    pbf_move_left_joystick(context, {0, 1}, 5000ms, 500ms);
    context.wait_for_all_requests();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
