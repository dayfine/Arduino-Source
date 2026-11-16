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

//  One slice of walk_to_prompt()'s walk between looks.
constexpr Milliseconds PROMPT_WALK_SLICE = 250ms;

//  After A at a prompt: how long a fade or text may take to show (a late
//  fade starts 1.8 s after the A), how often it is looked for, and how many
//  times A is pressed while the prompt stays with neither (a dropped A).
constexpr Milliseconds PROMPT_TAKEN_WAIT = 3000ms;
constexpr Milliseconds PROMPT_POLL = 250ms;
constexpr size_t PROMPT_PRESSES = 3;

//  Whether `look` shows that A at a prompt was taken: a fade, or text.
bool prompt_taken(const ScreenObservation& look){
    return look.events.has(ScreenEvent::FADE_BLACK) || look.events.has(ScreenEvent::FADE_WHITE) ||
        look.events.has(ScreenEvent::DIALOG_BOX) || look.events.has(ScreenEvent::CHOICE_BOX);
}

//  A at a prompt, then looks until a fade or text shows; returns whether
//  the prompt still shows on the last look without either.
bool press_at_prompt(PlzaBackend& backend){
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/400ms);
    bool prompt = true;
    for (Milliseconds waited = 0ms; waited < PROMPT_TAKEN_WAIT; waited += PROMPT_POLL){
        backend.scope().throw_if_cancelled();
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (prompt_taken(look)){
            return false;
        }
        prompt = look.events.has(ScreenEvent::INTERACT_PROMPT);
        backend.wait(PROMPT_POLL);
    }
    return prompt;
}

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

//  A sign that the player has control: the party bar, the station
//  platform after the opening (PlatformArrivalDetector), or the
//  tutorial's controls hint.
bool control_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) || look.readings.contains(PLATFORM_READING) ||
        look.readings.contains(CONTROLS_HINT_READING);
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
    const Milliseconds hold = move.legs.empty() ? 0ms : move.legs.back().walk;
    const Pokemon::Outcome walked = run_story_move_but_last_walk(backend, move);
    if (!walked.succeeded()){
        return walked;
    }
    if (hold == 0ms){
        //  A last leg that only turns (to face the clerk): what the walk
        //  goes to shows once the turn is done, or the walk missed it.
        if (backend.observe().events.has(end)){
            backend.log(std::format("[PLZA] {}: the end event after the last turn.", walk));
            return Pokemon::outcome_ok();
        }
        return Pokemon::outcome_failed("walk_no_end", std::format("{}: no end event after the last turn", walk));
    }
    //  The slices stop the stick between them for as long as a look takes;
    //  the player barely slows.
    for (Milliseconds held = 0ms; held < hold;){
        //  The last slice holds only what is left, so the leg walks its time.
        const Milliseconds slice = std::min(HOLD_SLICE, hold - held);
        hold_last_leg(backend, move, slice);
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

Pokemon::Outcome walk_to_prompt(PlzaBackend& backend, Milliseconds timeout){
    for (Milliseconds walked = 0ms; ; walked += PROMPT_WALK_SLICE){
        backend.scope().throw_if_cancelled();
        if (backend.observe().events.has(ScreenEvent::INTERACT_PROMPT)){
            backend.log(std::format("[PLZA] walk_to_prompt: the A prompt shows after {} ms of walking", walked.count()));
            return Pokemon::outcome_ok();
        }
        if (walked >= timeout){
            break;
        }
        backend.hold(Pokemon::Direction::NORTH, PROMPT_WALK_SLICE, Pokemon::StoryPace::WALK);
    }
    return Pokemon::outcome_failed("no_prompt", std::format("walk_to_prompt: no A prompt in {} ms of walking", timeout.count()));
}

bool prompt_shows_within(PlzaBackend& backend, Milliseconds within){
    for (Milliseconds waited = 0ms; ; waited += PROMPT_POLL){
        backend.scope().throw_if_cancelled();
        if (backend.observe().events.has(ScreenEvent::INTERACT_PROMPT)){
            return true;
        }
        if (waited >= within){
            return false;
        }
        backend.wait(PROMPT_POLL);
    }
}

void press_through_prompt(PlzaBackend& backend){
    for (size_t press = 1; press <= PROMPT_PRESSES; press++){
        if (!press_at_prompt(backend)){
            return;
        }
        backend.log(std::format("[PLZA] press_through_prompt: the prompt is still up {} ms after A {}", PROMPT_TAKEN_WAIT.count(), press));
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
