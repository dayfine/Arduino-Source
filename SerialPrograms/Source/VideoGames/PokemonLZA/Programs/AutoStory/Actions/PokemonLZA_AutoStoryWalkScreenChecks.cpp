/*  PokemonLZA AutoStory - Walk screen checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <iterator>
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

//  How long the last leg has been held. The leg is a time budget for its
//  end, not a distance: overshoot is harmless, and falls and fades take
//  game time. A run slice takes less time on a console than its length (the
//  backend runs it faster), so the leg is held until its time has passed on
//  the looks' clock too. A clock that stops (a script's repeated last look)
//  leaves the slices' lengths to end it.
class LastLegHold{
public:
    explicit LastLegHold(Milliseconds hold)
        : m_hold(hold)
    {}

    bool holding() const{
        return m_sliced < m_hold || (m_clock_runs && m_elapsed < m_hold);
    }
    //  The slices' lengths: the logs' and end_after's time.
    Milliseconds sliced() const{
        return m_sliced;
    }
    //  By the looks' clock, from the first slice; at least the budget once done.
    Milliseconds elapsed() const{
        return std::max(m_elapsed, m_hold);
    }

    void held_slice(Milliseconds slice, WallClock look_at){
        m_sliced += slice;
        if (!m_first_look){
            m_first_look = look_at;
            m_first_slice = slice;
        }else{
            m_clock_runs = look_at > m_last_look;
        }
        m_last_look = look_at;
        m_elapsed = m_first_slice + std::chrono::duration_cast<Milliseconds>(look_at - *m_first_look);
    }

private:
    Milliseconds m_hold;
    Milliseconds m_sliced = 0ms;
    Milliseconds m_elapsed = 0ms;
    Milliseconds m_first_slice = 0ms;
    std::optional<WallClock> m_first_look = std::nullopt;
    WallClock m_last_look = WallClock::min();
    bool m_clock_runs = false;
};

constexpr Milliseconds FREE_CONTROL_WAIT = 30000ms;
constexpr Milliseconds FREE_CONTROL_LOOK = 250ms;
//  Nothing busy this long, with no sign of control, is control where no
//  sign shows (Hotel Z's halls). Longer than the ~7 s from the end of
//  01.01's scenes to control.
constexpr Milliseconds QUIET_IS_CONTROL = 10000ms;

//  An A prompt shows only in the overworld, so it is control where no party
//  bar shows (01.03). Held a second, so a prompt left up under a late fade
//  after A at a door is not taken.
constexpr Milliseconds PROMPT_IS_CONTROL = 1000ms;

//  A first glimpse of the prompt can be it fading in or the player still
//  moving (02.05's elevator in German).
constexpr Milliseconds PROMPT_STEADY_GAP = 300ms;

bool prompt_taken(const ScreenObservation& look){
    return fade_shows(look) || text_shows(look) || look.events.has(ScreenEvent::CHOICE_BOX);
}

//  What keeps control from being free; none when it is.
std::optional<std::string_view> not_free_control(const ScreenObservation& look){
    if (text_shows(look)){
        return "text waiting for A";
    }
    if (fade_shows(look)){
        return "a fade";
    }
    return std::nullopt;
}

//  The party bar, or the tutorial's controls hint (also 01.01's platform,
//  which shows no party bar).
bool control_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) || look.readings.contains(CONTROLS_HINT_READING);
}

bool prompt_steady(PlzaBackend& backend){
    backend.wait(PROMPT_STEADY_GAP);
    return backend.observe().events.has(ScreenEvent::INTERACT_PROMPT);
}

//  The prompt can take seconds to come up after the walk to it (02.05's
//  elevator in German).
constexpr Milliseconds TALK_PROMPT_WAIT = 3000ms;
constexpr Milliseconds TALK_POLL = 250ms;

//  Two looks in a row: a walk that ends as it comes into reach can drift
//  out of it again, and an A on that one look is lost.
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

struct AtPrompt{
    //  What shows the game took A, and on how many looks in a row.
    bool (*until)(const ScreenObservation& look);
    size_t until_looks;
    //  As in all; how long after an A the prompt must still be up for the next.
    size_t retries;
    Milliseconds retry_gap;
    //  How long it looks after the first A. None: each A ends after
    //  `retry_gap` with the prompt gone.
    std::optional<Milliseconds> wait;
    Milliseconds release;
    std::string_view name;
};

enum class AtPromptEnd{
    TAKEN,
    PROMPT_STAYS,
    NOTHING,
};
struct AtPromptResult{
    AtPromptEnd end;
    size_t presses;
    Milliseconds since_press;
    ScreenObservation look;
};

//  TAKEN: `at.until` shows on `at.until_looks` looks in a row.
//  PROMPT_STAYS: still up after `at.retries` As. NOTHING: `at.wait` passed,
//  or with no `at.wait`, the prompt gone at the end of an A's `retry_gap`.
AtPromptResult a_at_prompt(PlzaBackend& backend, const AtPrompt& at){
    backend.press(BUTTON_A, /*hold=*/160ms, at.release);
    size_t presses = 1;
    Milliseconds since_press(0);
    size_t until_looks = 0;
    for (Milliseconds waited(0); ; ){
        backend.scope().throw_if_cancelled();
        ScreenObservation look = backend.observe();
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

const AtPrompt TALK{
    .until = scene_under_way,
    .until_looks = 2,
    .retries = 3,
    .retry_gap = 1000ms,
    .wait = 5000ms,
    .release = 240ms,
    .name = "talk",
};

//  A late fade starts 1.8 s after the A.
const AtPrompt THROUGH_PROMPT{
    .until = prompt_taken,
    .until_looks = 1,
    .retries = 3,
    .retry_gap = 3000ms,
    .wait = std::nullopt,
    .release = 400ms,
    .name = "press_through_prompt",
};

//  Sideways from the facing: 5 left, then 11 right, back past the start.
//  03.03b's ladder showed after 9 (console B, 2026-11-19).
constexpr double PROMPT_SIDESTEPS[] = {-90, -90, -90, -90, -90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90};

//  Only a walk of one straight leg to the first A prompt ahead; one that
//  turns, has earlier legs, or an `end_after` goes to a prompt elsewhere.
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

//  None: walk on. Set: the walk ends, with its failure if any.
std::optional<std::optional<Pokemon::StoryFailure>> end_event_look(
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
        return std::optional<Pokemon::StoryFailure>();
    }
    if (look.events.has(ScreenEvent::FADE_BLACK)){
        backend.log(std::format("[PLZA] {}: a black fade after {} ms of the last leg: the scene takes over.", walk, held.count()));
        return std::optional<Pokemon::StoryFailure>();
    }
    return std::nullopt;
}

//  A last leg that only turns (to face the clerk). No steps sideways when the
//  prompt is missing: the walk did not reach its target, and steps from the
//  wall beside 03.04's Center opening walked back out (console B 2026-11-19).
std::optional<Pokemon::StoryFailure> end_after_turn(PlzaBackend& backend, std::string_view walk, ScreenEvent end){
    if (backend.observe().events.has(end)){
        backend.log(std::format("[PLZA] {}: the end event after the last turn.", walk));
        return std::nullopt;
    }
    return Pokemon::StoryFailure{.code = "walk_no_end", .detail = std::format("{}: no end event after the last turn", walk)};
}

} // namespace


std::optional<Pokemon::StoryFailure> check_can_walk(PlzaBackend& backend, std::string_view walk){
    std::string_view last_busy = "nothing";
    int control_looks = 0;
    std::optional<Milliseconds> quiet_since;
    std::optional<Milliseconds> prompt_since;
    for (Milliseconds waited = 0ms; waited <= FREE_CONTROL_WAIT; waited += FREE_CONTROL_LOOK){
        const ScreenObservation look = backend.observe();
        const std::optional<std::string_view> busy = not_free_control(look);
        if (busy){
            last_busy = *busy;
            quiet_since.reset();
        }else if (!quiet_since){
            quiet_since = waited;
        }
        control_looks = !busy && control_shows(look) ? control_looks + 1 : 0;
        if (control_looks >= 2){
            return std::nullopt;
        }
        if (!busy && look.events.has(ScreenEvent::INTERACT_PROMPT)){
            if (!prompt_since){
                prompt_since = waited;
            }
        }else{
            prompt_since.reset();
        }
        if (prompt_since && waited - *prompt_since >= PROMPT_IS_CONTROL){
            backend.log(std::format("[PLZA] {}: the A prompt held {} ms with nothing busy: control.", walk, PROMPT_IS_CONTROL.count()));
            return std::nullopt;
        }
        if (quiet_since && waited - *quiet_since >= QUIET_IS_CONTROL){
            backend.log(std::format(
                "[PLZA] {}: nothing busy for {} ms and no sign of control; taken as control.", walk, QUIET_IS_CONTROL.count()
            ));
            return std::nullopt;
        }
        backend.wait(FREE_CONTROL_LOOK);
    }
    return Pokemon::StoryFailure{.code = "walk_not_free", .detail =
        std::format("{}: no free control within {} ms, last on {}", walk, FREE_CONTROL_WAIT.count(), last_busy)
    };
}

void finish_walk(PlzaBackend& backend, std::string_view walk){
    const ScreenObservation look = backend.observe();
    backend.log(std::format("[PLZA] {}: legs done, screen now: {}", walk, look.events));
}

std::optional<Pokemon::StoryFailure> walk_until(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, std::optional<ScreenEvent> end, Milliseconds end_after
){
    if (end && ends_before_walking(backend, walk, move, *end, end_after)){
        return std::nullopt;
    }
    const Milliseconds hold = move.legs.empty() ? 0ms : move.legs.back().walk;
    const std::optional<Pokemon::StoryFailure> walked = run_story_move_but_last_walk(backend, move);
    if (walked){
        return walked;
    }
    if (hold == 0ms && end){
        return end_after_turn(backend, walk, *end);
    }
    size_t scene_looks = 0;
    LastLegHold leg(hold);
    //  The slices stop the stick only as long as a look takes.
    while (leg.holding()){
        backend.scope().throw_if_cancelled();
        const Milliseconds slice = leg.sliced() < hold ? std::min(HOLD_SLICE, hold - leg.sliced()) : HOLD_SLICE;
        hold_last_leg(backend, move, slice);
        const ScreenObservation look = backend.observe();
        leg.held_slice(slice, look.when);
        const Milliseconds held = leg.sliced();
        if (!end){
            const std::optional<std::string_view> busy = not_free_control(look);
            scene_looks = busy ? scene_looks + 1 : 0;
            if (busy && scene_looks >= 2){
                backend.log(std::format(
                    "[PLZA] {}: a scene ({}) took control after {} ms of the last leg: the walk stops.", walk, *busy, held.count()
                ));
                return std::nullopt;
            }
            continue;
        }
        const std::optional<std::optional<Pokemon::StoryFailure>> ended = end_event_look(backend, walk, look, *end, held, end_after);
        if (ended){
            return *ended;
        }
    }
    if (!end){
        return Pokemon::StoryFailure{.code = "walk_no_scene", .detail = std::format("{}: control still free after {} ms of the last leg: no scene took it", walk, leg.elapsed().count())};
    }
    return Pokemon::StoryFailure{.code = "walk_no_end", .detail = std::format("{}: no end event within {} ms of the last leg", walk, leg.elapsed().count())};
}

std::optional<size_t> step_along_until_prompt(PlzaBackend& backend, double facing_degrees){
    for (size_t steps = 0; ; steps++){
        backend.scope().throw_if_cancelled();
        if (backend.observe().events.has(ScreenEvent::INTERACT_PROMPT)){
            return steps;
        }
        if (steps == std::size(PROMPT_SIDESTEPS)){
            return std::nullopt;
        }
        backend.stick(facing_degrees + PROMPT_SIDESTEPS[steps], /*duration=*/250ms, /*release=*/100ms, Pokemon::StoryPace::WALK);
        backend.stick(facing_degrees, /*duration=*/120ms, /*release=*/500ms, Pokemon::StoryPace::WALK);
    }
}

std::optional<Pokemon::StoryFailure> talk_at_prompt(PlzaBackend& backend){
    if (!talk_prompt_settled(backend)){
        return Pokemon::StoryFailure{.code = "talk_no_prompt", .detail = std::format("no A prompt on two looks within {} ms: not at what the talk is for", TALK_PROMPT_WAIT.count())};
    }
    const AtPromptResult result = a_at_prompt(backend, TALK);
    switch (result.end){
    case AtPromptEnd::TAKEN:
        backend.log_detail(std::format(
            "[PLZA] talk: started {} ms after A {}: {}", result.since_press.count(), result.presses, describe_scene_look(result.look)
        ));
        return std::nullopt;
    case AtPromptEnd::PROMPT_STAYS:
        return Pokemon::StoryFailure{.code = "talk_no_scene", .detail = std::format(
            "the A prompt still up with nothing open after {} As: the game took none", result.presses
        )};
    case AtPromptEnd::NOTHING:
        break;
    }
    return Pokemon::StoryFailure{.code = "talk_no_scene", .detail = std::format("nothing started within {} ms of A: the HUD stayed", TALK.wait->count())};
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
