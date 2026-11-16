/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  How long the map talk may take, and how long the party bar must hold.
constexpr Milliseconds MAP_TALK_TIMEOUT = 120s;
constexpr Milliseconds MAP_TALK_CONTROL_HOLD = 1000ms;
//  Each button's press and the wait after it.
constexpr Milliseconds MAP_TALK_HOLD = 160ms;
constexpr Milliseconds MAP_TALK_RELEASE = 840ms;
//  How often it looks while the party bar holds.
constexpr Milliseconds MAP_TALK_POLL = 250ms;

//  Free control: the party bar, with no text box over it (it shows between
//  the lines of a talk).
bool shows_control(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) && !look.events.has(ScreenEvent::DIALOG_BOX);
}

} // namespace


Pokemon::Outcome talk_through_map_screen(PlzaBackend& backend){
    //  Each look presses for 2 s or waits; the looks bound a script whose
    //  time stands still.
    const size_t max_looks = (size_t)(MAP_TALK_TIMEOUT / MAP_TALK_POLL) + 1;
    std::optional<WallClock> start;
    std::optional<WallClock> control_since;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        start = start.value_or(look.when);
        if (shows_control(look)){
            control_since = control_since.value_or(look.when);
            if (look.when - *control_since >= MAP_TALK_CONTROL_HOLD){
                backend.log("[PLZA] talk_through_map_screen: the party bar held: back in the overworld");
                return Pokemon::outcome_ok();
            }
            backend.wait(MAP_TALK_POLL);
            continue;
        }
        control_since.reset();
        if (look.when - *start >= MAP_TALK_TIMEOUT){
            break;
        }
        backend.press(BUTTON_A, MAP_TALK_HOLD, MAP_TALK_RELEASE);
        backend.press(BUTTON_B, MAP_TALK_HOLD, MAP_TALK_RELEASE);
    }
    return Pokemon::outcome_failed("map_talk_timeout", std::format(
        "no party bar held {} ms within {} ms of A and B in turn",
        MAP_TALK_CONTROL_HOLD.count(), MAP_TALK_TIMEOUT.count()
    ));
}

void climb_ladder(PlzaBackend& backend){
    backend.stick(/*degrees=*/90, /*duration=*/600ms, /*release=*/300ms, Pokemon::StoryPace::WALK);
    backend.stick(/*degrees=*/0, /*duration=*/150ms, /*release=*/300ms, Pokemon::StoryPace::WALK);
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/1000ms);
    backend.stick(/*degrees=*/0, /*duration=*/5000ms, /*release=*/500ms, Pokemon::StoryPace::WALK);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
