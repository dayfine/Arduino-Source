/*  PokemonLZA AutoStory - Walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenTimers.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenWait.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenPredicates.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryWalks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

void story_turn(PlzaBackend& backend, double degrees){
    //  The tap the story was recorded with; short releases run legs back to back.
    backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
    backend.stick(degrees, /*duration=*/150ms, /*release=*/100ms, Pokemon::StoryPace::WALK);
}

//  A dropped turn or hold leaves the screen still. A walk into a wall reads
//  still too, and plays its leg twice.
bool screen_still(PlzaBackend& backend){
    return backend.observe().events.has(ScreenEvent::SCREEN_STILL);
}

//  L since the walk goes where the camera looks.
void turn_and_recenter(PlzaBackend& backend, double degrees){
    story_turn(backend, degrees);
    backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
}

template <typename Input>
void play_checked(PlzaBackend& backend, std::string_view what, Input&& input){
    input();
    if (screen_still(backend)){
        backend.log(std::format("[PLZA] walk: the screen stayed still after {}; once more", what));
        input();
    }
}

void recenter_before_held_leg(PlzaBackend& backend, size_t index, const StoryWalkLeg& leg){
    if (index > 0 && leg.turn != 0){
        backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
    }
}

void run_held_legs(PlzaBackend& backend, const std::vector<StoryWalkLeg>& legs){
    for (size_t index = 0; index < legs.size(); index++){
        const StoryWalkLeg& leg = legs[index];
        recenter_before_held_leg(backend, index, leg);
        if (leg.walk > 0ms){
            backend.stick(leg.turn, leg.walk, /*release=*/0ms, Pokemon::StoryPace::RUN);
        }
    }
}

//  "leg 2/5 turn 90 walk 1200 ms"
std::string story_walk_leg_label(size_t index, size_t count, const StoryWalkLeg& leg){
    std::string label = std::format("leg {}/{} turn {:g}", index + 1, count, leg.turn);
    if (leg.walk > Milliseconds(0)){
        label += std::format(" walk {} ms", leg.walk.count());
    }
    return label;
}

void run_story_move_but_last_walk(PlzaBackend& backend, const StoryMove& move){
    if (move.legs.empty()){
        return;
    }
    StoryMove head = move;
    if (move.style == StoryWalkStyle::HELD){
        head.legs.pop_back();
        run_held_legs(backend, head.legs);
        recenter_before_held_leg(backend, head.legs.size(), move.legs.back());
        return;
    }
    head.legs.back().walk = 0ms;
    run_story_move(backend, head);
}

void hold_last_leg(PlzaBackend& backend, const StoryMove& move, Milliseconds slice){
    if (move.style == StoryWalkStyle::HELD && !move.legs.empty()){
        backend.stick(move.legs.back().turn, slice, /*release=*/0ms, Pokemon::StoryPace::RUN);
    }else{
        backend.stick(/*degrees=*/0, slice, /*release=*/0ms, Pokemon::StoryPace::RUN);
    }
}

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
//  sign shows. Longer than the ~7 s from the end of 01.01's scenes to control.
constexpr Milliseconds QUIET_IS_CONTROL = 10000ms;

//  An A prompt shows only in the overworld, so it is control where no
//  party bar or hint shows (01.02). Held a second, so a prompt left up
//  under a late fade after A is not taken.
constexpr Milliseconds PROMPT_IS_CONTROL = 1000ms;

//  What keeps control from being free; none when it is.
std::optional<std::string_view> not_free_control(const ScreenObservation& look){
    if (look.readings.contains(ADVANCE_ARROW_READING)){
        return "text waiting for A";
    }
    if (look.events.has(ScreenEvent::DIALOG_BOX)){
        return "a text box";
    }
    if (FADE_SHOWS.matches(look)){
        return "a fade";
    }
    return std::nullopt;
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

struct FreeControlWatch{
    std::string_view last_busy = "nothing";
    Pokemon::ConfirmStreak control_streak{2};
    Pokemon::HoldTimer quiet_hold{QUIET_IS_CONTROL};
    Pokemon::HoldTimer prompt_hold{PROMPT_IS_CONTROL};
    size_t poll_looks = 0;
};

bool check_free_control(PlzaBackend& backend, std::string_view walk, const ScreenObservation& look, FreeControlWatch& watch){
    const WallClock poll_time = WallClock::min() + watch.poll_looks * FREE_CONTROL_LOOK;
    watch.poll_looks++;
    const std::optional<std::string_view> busy = not_free_control(look);
    if (busy){
        watch.last_busy = *busy;
    }
    const bool quiet_ready = watch.quiet_hold.update(!busy.has_value(), poll_time);
    if (watch.control_streak.update(OVERWORLD_FREE.matches(look))){
        return true;
    }
    if (watch.prompt_hold.update(!busy && look.events.has(ScreenEvent::INTERACT_PROMPT), poll_time)){
        backend.log(std::format("[PLZA] {}: the A prompt held {} ms with nothing busy: control.", walk, PROMPT_IS_CONTROL.count()));
        return true;
    }
    if (quiet_ready){
        backend.log(std::format(
            "[PLZA] {}: nothing busy for {} ms and no sign of control; taken as control.", walk, QUIET_IS_CONTROL.count()
        ));
        return true;
    }
    return false;
}

struct LastLegOutcome{
    std::optional<std::optional<Pokemon::StoryFailure>> ended;
    Milliseconds sliced = 0ms;
    Milliseconds elapsed = 0ms;
};

//  Runs all legs but the last walk, then slices the last leg until `on_look` ends it
//  or the hold budget elapses.
template <typename OnLook>
LastLegOutcome walk_last_leg(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, OnLook&& on_look
){
    const Milliseconds hold = move.legs.empty() ? 0ms : move.legs.back().walk;
    run_story_move_but_last_walk(backend, move);
    LastLegHold leg(hold);
    LastLegOutcome outcome;
    //  The slices stop the stick only as long as a look takes.
    while (leg.holding()){
        backend.scope().throw_if_cancelled();
        const Milliseconds slice = leg.sliced() < hold ? std::min(HOLD_SLICE, hold - leg.sliced()) : HOLD_SLICE;
        hold_last_leg(backend, move, slice);
        const ScreenObservation look = backend.observe();
        leg.held_slice(slice, look.when);
        if (std::optional<std::optional<Pokemon::StoryFailure>> ended = on_look(look, leg.sliced())){
            outcome.ended = std::move(ended);
            return outcome;
        }
    }
    outcome.sliced = leg.sliced();
    outcome.elapsed = leg.elapsed();
    return outcome;
}

} // namespace


void run_story_move(PlzaBackend& backend, const StoryMove& move){
    if (move.style == StoryWalkStyle::HELD){
        run_held_legs(backend, move.legs);
        return;
    }
    //  The look the first input is checked against.
    backend.observe();
    for (size_t index = 0; index < move.legs.size(); index++){
        const StoryWalkLeg& leg = move.legs[index];
        const std::string label = story_walk_leg_label(index, move.legs.size(), leg);
        if (leg.turn != 0){
            play_checked(backend, std::format("the turn of {}", label), [&]{
                turn_and_recenter(backend, leg.turn);
            });
        }
        if (leg.walk > Milliseconds(0)){
            play_checked(backend, std::format("the walk of {}", label), [&]{
                backend.stick(/*degrees=*/0, leg.walk, /*release=*/0ms, Pokemon::StoryPace::RUN);
            });
        }
    }
}

std::optional<Pokemon::StoryFailure> check_can_walk(PlzaBackend& backend, std::string_view walk){
    FreeControlWatch watch;
    const Pokemon::WaitOutcome outcome = Pokemon::wait_for(
        backend,
        [&](const ScreenObservation& look){
            return check_free_control(backend, walk, look, watch);
        },
        Pokemon::WaitRule{.poll = FREE_CONTROL_LOOK, .timeout = FREE_CONTROL_WAIT}
    );
    if (const auto* failure = std::get_if<Pokemon::StoryFailure>(&outcome)){
        return *failure;
    }
    if (std::holds_alternative<Pokemon::WaitMatched>(outcome)){
        return std::nullopt;
    }
    return Pokemon::StoryFailure{.code = "walk_not_free", .detail =
        std::format("{}: no free control within {} ms, last on {}", walk, FREE_CONTROL_WAIT.count(), watch.last_busy)
    };
}

void finish_walk(PlzaBackend& backend, std::string_view walk){
    const ScreenObservation look = backend.observe();
    backend.log(std::format("[PLZA] {}: legs done, screen now: {}", walk, look.events));
}

std::optional<Pokemon::StoryFailure> walk_until(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, const WalkStopOptions& options
){
    size_t scene_looks = 0;
    const LastLegOutcome walked = walk_last_leg(
        backend, walk, move,
        [&](const ScreenObservation& look, Milliseconds held) -> std::optional<std::optional<Pokemon::StoryFailure>>{
            if (!options.end){
                const std::optional<std::string_view> busy = not_free_control(look);
                scene_looks = busy ? scene_looks + 1 : 0;
                if (busy && scene_looks >= 2){
                    backend.log(std::format(
                        "[PLZA] {}: a scene ({}) took control after {} ms of the last leg: the walk stops.", walk, *busy, held.count()
                    ));
                    return std::optional<Pokemon::StoryFailure>();
                }
                return std::nullopt;
            }
            return end_event_look(backend, walk, look, *options.end, held, options.end_after);
        }
    );
    if (walked.ended){
        return *walked.ended;
    }
    if (!options.end){
        return Pokemon::StoryFailure{.code = "walk_no_scene", .detail = std::format("{}: control still free after {} ms of the last leg: no scene took it", walk, walked.elapsed.count())};
    }
    return Pokemon::StoryFailure{.code = "walk_no_end", .detail = std::format("{}: no end event within {} ms of the last leg", walk, walked.elapsed.count())};
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
