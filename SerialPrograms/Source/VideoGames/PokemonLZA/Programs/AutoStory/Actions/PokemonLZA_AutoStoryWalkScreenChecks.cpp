/*  PokemonLZA AutoStory - Walk screen checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryWalkScreenChecks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  How long each slice of the held last leg is, between looks for its end.
constexpr Milliseconds HOLD_SLICE = 250ms;

//  How long a walk waits for free control, and how often it looks.
constexpr Milliseconds FREE_CONTROL_WAIT = 30000ms;
constexpr Milliseconds FREE_CONTROL_LOOK = 250ms;
//  Nothing busy for this long, with no sign of control, is taken as
//  control, for places that show no sign (Hotel Z's halls show no party
//  bar). Longer than the ~7 s from the end of 01.01's scenes to control
//  (console B, 2026-11-15).
constexpr Milliseconds QUIET_IS_CONTROL = 10000ms;

//  What no walk may start on, by name; empty when free.
std::string_view not_free_control(const ScreenObservation& look){
    if (look.readings.contains(ADVANCE_ARROW_READING)){
        return "text waiting for A";
    }
    if (look.events.has(ScreenEvent::DIALOG_BOX)){
        return "a text box";
    }
    if (look.events.has(ScreenEvent::FADE_BLACK) || look.events.has(ScreenEvent::FADE_WHITE)){
        return "a fade";
    }
    return "";
}

//  A sign that the player has control: the party bar, or the station
//  platform after the opening (PlatformArrivalDetector).
bool control_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) || look.readings.contains(PLATFORM_READING);
}

} // namespace


Pokemon::Outcome check_can_walk(PlzaBackend& backend, std::string_view walk){
    std::string_view last_busy = "nothing";
    int control_looks = 0;
    std::optional<Milliseconds> quiet_since;
    for (Milliseconds waited = 0ms; waited <= FREE_CONTROL_WAIT; waited += FREE_CONTROL_LOOK){
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        const std::string_view busy = not_free_control(look);
        if (!busy.empty()){
            last_busy = busy;
            quiet_since.reset();
        }else if (!quiet_since){
            quiet_since = waited;
        }
        control_looks = busy.empty() && control_shows(look) ? control_looks + 1 : 0;
        if (control_looks >= 2){
            return Pokemon::outcome_ok();
        }
        if (quiet_since && waited - *quiet_since >= QUIET_IS_CONTROL){
            backend.log(std::format(
                "[PLZA] {}: nothing busy for {} ms and no sign of control; taken as control.", walk, QUIET_IS_CONTROL.count()
            ));
            return Pokemon::outcome_ok();
        }
        backend.wait(FREE_CONTROL_LOOK);
    }
    return Pokemon::outcome_failed(
        "walk_not_free",
        std::format("{}: no free control within {} ms, last on {}", walk, FREE_CONTROL_WAIT.count(), last_busy)
    );
}

void finish_walk(PlzaBackend& backend, std::string_view walk){
    const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
    backend.log(std::format("[PLZA] {}: legs done, screen now: {}", walk, describe(look.events)));
}

Pokemon::Outcome walk_until(PlzaBackend& backend, std::string_view walk, const StoryMove& move, ScreenEvent end){
    StoryMove head = move;
    Milliseconds hold = 0ms;
    if (!head.legs.empty()){
        hold = head.legs.back().walk;
        head.legs.back().walk = 0ms;
    }
    const Pokemon::Outcome walked = run_story_move(backend, head);
    if (!walked.succeeded()){
        return walked;
    }
    //  The slices stop the stick between them for as long as a look takes;
    //  the player barely slows.
    for (Milliseconds held = 0ms; held < hold;){
        //  The last slice holds only what is left, so the leg walks its time.
        const Milliseconds slice = std::min(HOLD_SLICE, hold - held);
        backend.hold(Pokemon::Direction::NORTH, slice, Pokemon::StoryPace::WALK);
        held += slice;
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (look.events.has(end)){
            backend.log(std::format("[PLZA] {}: the end event after {} ms of the last leg.", walk, held.count()));
            return Pokemon::outcome_ok();
        }
        if (look.events.has(ScreenEvent::FADE_BLACK)){
            backend.log(std::format("[PLZA] {}: a black fade after {} ms of the last leg: the scene takes over.", walk, held.count()));
            return Pokemon::outcome_ok();
        }
    }
    return Pokemon::outcome_failed("walk_no_end", std::format("{}: no end event within {} ms of the last leg", walk, hold.count()));
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
