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
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
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

//  An A prompt held this long with nothing busy is control: it shows only
//  in the overworld, and where no party bar shows (01.03 before the
//  starter has no party) it is the one sign there is. A second, as the scene machine holds it, so a
//  prompt left up under a late fade after A at a door is not taken.
constexpr Milliseconds PROMPT_IS_CONTROL = 1000ms;

//  A walk to an A prompt ends on it only when it shows again this long
//  after a look that saw it, the stick let go: a first glimpse can be the
//  prompt fading in or the player still moving, and the talk after it
//  then finds none (02.05's elevator in German, console B 2026-11-16).
constexpr Milliseconds PROMPT_STEADY_GAP = 300ms;

//  Whether `look` shows that A at a door's prompt was taken: a fade, or
//  text.
bool prompt_taken(const ScreenObservation& look){
    return fade_shows(look) || look.events.has(ScreenEvent::DIALOG_BOX) || look.events.has(ScreenEvent::CHOICE_BOX);
}

//  What no walk may start on, by name; empty when free.
std::string_view not_free_control(const ScreenObservation& look){
    if (look.readings.contains(ADVANCE_ARROW_READING)){
        return "text waiting for A";
    }
    if (look.events.has(ScreenEvent::DIALOG_BOX)){
        return "a text box";
    }
    if (fade_shows(look)){
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

//  Whether the A prompt a look just saw shows again PROMPT_STEADY_GAP
//  later, the player standing.
bool prompt_steady(PlzaBackend& backend){
    backend.wait(PROMPT_STEADY_GAP);
    return backend.observe().events.has(ScreenEvent::INTERACT_PROMPT);
}

//  How long "talk" looks for its A prompt before A, and how often it looks.
//  The prompt can take seconds to come up after the walk to it (02.05's
//  elevator in German, console B 2026-11-16: none within 1 s, steady a
//  few seconds later).
constexpr Milliseconds TALK_PROMPT_WAIT = 3000ms;
constexpr Milliseconds TALK_POLL = 250ms;

//  Whether the A prompt shows on two looks in a row within
//  TALK_PROMPT_WAIT: a walk that ends as it comes into reach can drift out
//  of it again, and an A on that one look is lost (02.05's elevator,
//  console B 2026-11-16).
bool talk_prompt_settled(PlzaBackend& backend){
    size_t prompt_looks = 0;
    for (Milliseconds waited(0); ; waited += TALK_POLL){
        backend.scope().throw_if_cancelled();
        prompt_looks = backend.observe().events.has(ScreenEvent::INTERACT_PROMPT) ? prompt_looks + 1 : 0;
        if (prompt_looks >= 2){
            return true;
        }
        if (waited >= TALK_PROMPT_WAIT){
            return false;
        }
        backend.wait(TALK_POLL);
    }
}

//  How a_at_prompt() goes on after A at the A prompt the player stands at.
struct AtPrompt{
    //  What shows the game took A, and on how many looks in a row.
    bool (*until)(const ScreenObservation& look);
    size_t until_looks;
    //  As in all, and how long after an A the prompt must still be up
    //  with `until` not shown for the next.
    size_t retries;
    Milliseconds retry_gap;
    //  How long it looks after the first A, at most. None: each A gets
    //  `retry_gap`, decided on its last look, and ends there with the
    //  prompt gone.
    std::optional<Milliseconds> wait;
    //  The release after each A.
    Milliseconds release;
    //  The name it logs an A again under.
    std::string_view name;
};

//  How a_at_prompt() ended, after how many As, and its last look.
enum class AtPromptEnd{
    TAKEN,
    PROMPT_STAYS,
    NOTHING,
};
struct AtPromptResult{
    AtPromptEnd end;
    size_t presses;
    Milliseconds since_press;
    Pokemon::Observation<Pokemon::ZonePose> look;
};

//  A at a prompt, then a look every TALK_POLL until `at.until` shows on
//  `at.until_looks` looks in a row (TAKEN); while the prompt stays up
//  with nothing open `at.retry_gap` after A, A again, `at.retries` As at
//  most (then PROMPT_STAYS). NOTHING: `at.wait` passed, or with no
//  `at.wait`, the prompt gone at the end of an A's `at.retry_gap`.
AtPromptResult a_at_prompt(PlzaBackend& backend, const AtPrompt& at){
    backend.press(BUTTON_A, /*hold=*/160ms, at.release);
    size_t presses = 1;
    Milliseconds since_press(0);
    size_t until_looks = 0;
    for (Milliseconds waited(0); ; ){
        backend.scope().throw_if_cancelled();
        Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        until_looks = at.until(look) ? until_looks + 1 : 0;
        if (until_looks >= at.until_looks){
            return {AtPromptEnd::TAKEN, presses, since_press, std::move(look)};
        }
        const bool prompt_stays = until_looks == 0 && look.events.has(ScreenEvent::INTERACT_PROMPT);
        bool again = false;
        if (!at.wait && since_press + TALK_POLL >= at.retry_gap){
            backend.wait(TALK_POLL);
            since_press += TALK_POLL;
            if (!prompt_stays){
                return {AtPromptEnd::NOTHING, presses, since_press, std::move(look)};
            }
            again = true;
        }
        if (at.wait && prompt_stays && since_press >= at.retry_gap){
            again = true;
        }
        if (again){
            if (presses >= at.retries){
                return {AtPromptEnd::PROMPT_STAYS, presses, since_press, std::move(look)};
            }
            backend.log(std::format(
                "[PLZA] {}: the A prompt still up {} ms after A {} with nothing open; A again", at.name, since_press.count(), presses
            ));
            backend.press(BUTTON_A, /*hold=*/160ms, at.release);
            presses++;
            since_press = Milliseconds(0);
            continue;
        }
        if (at.wait && waited >= *at.wait){
            return {AtPromptEnd::NOTHING, presses, since_press, std::move(look)};
        }
        backend.wait(TALK_POLL);
        waited += TALK_POLL;
        since_press += TALK_POLL;
    }
}

//  "talk"'s A: its scene under way on two looks in a row within 5 s, A
//  again every second the prompt stays up with nothing open, 3 As at most.
const AtPrompt TALK{
    .until = scene_under_way,
    .until_looks = 2,
    .retries = 3,
    .retry_gap = 1000ms,
    .wait = 5000ms,
    .release = 240ms,
    .name = "talk",
};

//  A door's A: a fade or text within 3 s (a late fade starts 1.8 s after
//  the A), A again while the prompt stays with neither (a dropped A), 3
//  As at most.
const AtPrompt THROUGH_PROMPT{
    .until = prompt_taken,
    .until_looks = 1,
    .retries = 3,
    .retry_gap = 3000ms,
    .wait = std::nullopt,
    .release = 400ms,
    .name = "press_through_prompt",
};

//  Whether walk_until() of `move` to `end` from `end_after` ends before
//  its first leg on the A prompt it starts at: a walk of one straight leg
//  to the first A prompt ahead. A walk that turns or has legs before its
//  last goes to a prompt elsewhere, and one with an `end_after` passes
//  prompts on its way.
bool ends_before_walking(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, ScreenEvent end, Milliseconds end_after
){
    const bool straight_to_prompt = end == ScreenEvent::INTERACT_PROMPT && end_after == 0ms &&
        move.legs.size() == 1 && move.legs[0].turn == 0;
    if (!straight_to_prompt || !backend.observe().events.has(end) || !prompt_steady(backend)){
        return false;
    }
    backend.log(std::format("[PLZA] {}: the A prompt shows before the walk: no walk.", walk));
    return true;
}

//  What a look `held` into a walk's last leg says for walk_until() with
//  the event `end`: how the walk ends, or none to walk on.
std::optional<Pokemon::Outcome> end_event_look(
    PlzaBackend& backend, std::string_view walk, const ScreenObservation& look,
    ScreenEvent end, Milliseconds held, Milliseconds end_after
){
    if (look.events.has(end) && held < end_after){
        backend.log(std::format(
            "[PLZA] {}: the end event after {} ms of the last leg, before its {} ms: passed on the way, on.",
            walk, held.count(), end_after.count()
        ));
        return std::nullopt;
    }
    if (look.events.has(end) && end == ScreenEvent::INTERACT_PROMPT && !prompt_steady(backend)){
        backend.log(std::format("[PLZA] {}: the A prompt after {} ms of the last leg went again: on.", walk, held.count()));
        return std::nullopt;
    }
    if (look.events.has(end)){
        backend.log(std::format("[PLZA] {}: the end event after {} ms of the last leg.", walk, held.count()));
        backend.log_detail(std::format("[PLZA] {}: ended on: {}", walk, describe_scene_look(look)));
        return Pokemon::outcome_ok();
    }
    if (look.events.has(ScreenEvent::FADE_BLACK)){
        backend.log(std::format("[PLZA] {}: a black fade after {} ms of the last leg: the scene takes over.", walk, held.count()));
        return Pokemon::outcome_ok();
    }
    return std::nullopt;
}

//  A last leg that only turns (to face the clerk): what the walk goes to
//  shows once the turn is done, or the walk missed it.
Pokemon::Outcome end_after_turn(PlzaBackend& backend, std::string_view walk, ScreenEvent end){
    if (backend.observe().events.has(end)){
        backend.log(std::format("[PLZA] {}: the end event after the last turn.", walk));
        return Pokemon::outcome_ok();
    }
    return Pokemon::outcome_failed("walk_no_end", std::format("{}: no end event after the last turn", walk));
}

} // namespace


Pokemon::Outcome check_can_walk(PlzaBackend& backend, std::string_view walk){
    std::string_view last_busy = "nothing";
    int control_looks = 0;
    std::optional<Milliseconds> quiet_since;
    std::optional<Milliseconds> prompt_since;
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
        if (busy.empty() && look.events.has(ScreenEvent::INTERACT_PROMPT)){
            if (!prompt_since){
                prompt_since = waited;
            }
        }else{
            prompt_since.reset();
        }
        if (prompt_since && waited - *prompt_since >= PROMPT_IS_CONTROL){
            backend.log(std::format("[PLZA] {}: the A prompt held {} ms with nothing busy: control.", walk, PROMPT_IS_CONTROL.count()));
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

Pokemon::Outcome walk_until(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, std::optional<ScreenEvent> end, Milliseconds end_after
){
    if (end && ends_before_walking(backend, walk, move, *end, end_after)){
        return Pokemon::outcome_ok();
    }
    const Milliseconds hold = move.legs.empty() ? 0ms : move.legs.back().walk;
    const Pokemon::Outcome walked = run_story_move_but_last_walk(backend, move);
    if (!walked.succeeded()){
        return walked;
    }
    if (hold == 0ms && end){
        return end_after_turn(backend, walk, *end);
    }
    size_t scene_looks = 0;
    //  The slices stop the stick between them for as long as a look takes;
    //  the player barely slows.
    for (Milliseconds held = 0ms; held < hold;){
        backend.scope().throw_if_cancelled();
        //  The last slice holds only what is left, so the leg walks its time.
        const Milliseconds slice = std::min(HOLD_SLICE, hold - held);
        hold_last_leg(backend, move, slice);
        held += slice;
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (!end){
            const std::string_view busy = not_free_control(look);
            scene_looks = busy.empty() ? 0 : scene_looks + 1;
            if (scene_looks >= 2){
                backend.log(std::format(
                    "[PLZA] {}: a scene ({}) took control after {} ms of the last leg: the walk stops.", walk, busy, held.count()
                ));
                return Pokemon::outcome_ok();
            }
            continue;
        }
        const std::optional<Pokemon::Outcome> ended = end_event_look(backend, walk, look, *end, held, end_after);
        if (ended){
            return *ended;
        }
    }
    if (!end){
        return Pokemon::outcome_failed(
            "walk_no_scene", std::format("{}: control still free after {} ms of the last leg: no scene took it", walk, hold.count())
        );
    }
    return Pokemon::outcome_failed("walk_no_end", std::format("{}: no end event within {} ms of the last leg", walk, hold.count()));
}

Pokemon::Outcome talk_at_prompt(PlzaBackend& backend){
    if (!talk_prompt_settled(backend)){
        return Pokemon::outcome_failed(
            "talk_no_prompt", std::format("no A prompt on two looks within {} ms: not at what the talk is for", TALK_PROMPT_WAIT.count())
        );
    }
    const AtPromptResult result = a_at_prompt(backend, TALK);
    switch (result.end){
    case AtPromptEnd::TAKEN:
        backend.log_detail(std::format(
            "[PLZA] talk: started {} ms after A {}: {}", result.since_press.count(), result.presses, describe_scene_look(result.look)
        ));
        return Pokemon::outcome_ok();
    case AtPromptEnd::PROMPT_STAYS:
        return Pokemon::outcome_failed("talk_no_scene", std::format(
            "the A prompt still up with nothing open after {} As: the game took none", result.presses
        ));
    case AtPromptEnd::NOTHING:
        break;
    }
    return Pokemon::outcome_failed("talk_no_scene", std::format("nothing started within {} ms of A: the HUD stayed", TALK.wait->count()));
}

void press_through_prompt(PlzaBackend& backend){
    const AtPromptResult result = a_at_prompt(backend, THROUGH_PROMPT);
    if (result.end == AtPromptEnd::PROMPT_STAYS){
        backend.log(std::format(
            "[PLZA] press_through_prompt: the prompt is still up {} ms after A {}", result.since_press.count(), result.presses
        ));
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
