/*  PokemonLZA AutoStory - Platform wait
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <variant>
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenTimers.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenWait.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryReadings.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenPredicates.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySignalProbe.h"
#include "PokemonLZA_AutoStoryPlatformWait.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::StoryFailure;


namespace{

//  The autosave badge shows for 1-3 s: at least 2 looks a second.
constexpr Milliseconds PLATFORM_POLL(300);
constexpr size_t PLATFORM_LOOKS = 2;

//  A Plus held while the confirm screen still fades out does nothing (console
//  B, 2026-11-19: held 4 ms into the step). The opening's "hold to skip"
//  takes one this long after its picture fades in.
constexpr Milliseconds OPENING_SKIP_AFTER_FADE_IN = 750ms;

const Pokemon::ScreenMatch AUTOSAVE_SHOWS{
    .all_readings = {AUTOSAVE_READING},
};

Milliseconds elapsed_since(WallClock start, WallClock now){
    if (now <= start){
        return 0ms;
    }
    return std::chrono::duration_cast<Milliseconds>(now - start);
}

struct OpeningProgress{
    //  On the last look.
    Pokemon::EdgeTrigger text_waiting;
    Pokemon::EdgeTrigger fade_shown;
    //  When the picture came back after a fade, until Plus is held for it.
    std::optional<WallClock> faded_in = std::nullopt;
};

void track_fade_in(const ScreenObservation& look, OpeningProgress& progress){
    const bool fade = FADE_SHOWS.matches(look);
    if (fade){
        progress.faded_in.reset();
    }
    if (progress.fade_shown.falling(fade)){
        progress.faded_in = look.when;
    }
}

//  Once per fade-in, while the opening plays. Plus on the platform opens the
//  map, so none once its controls hint shows (and this phase ends on autosave).
bool opening_skip_due(const ScreenObservation& look, const OpeningProgress& progress){
    if (!progress.faded_in){
        return false;
    }
    return elapsed_since(*progress.faded_in, look.when) >= OPENING_SKIP_AFTER_FADE_IN &&
        !TEXT_SHOWS.matches(look) &&
        !look.readings.contains(CONTROLS_HINT_READING);
}

void advance_opening(PlzaBackend& backend, const ScreenObservation& look, OpeningProgress& progress){
    if (progress.text_waiting.rising(TEXT_WAITING_FOR_A.matches(look))){
        backend.log("[PLZA] platform wait: text waiting for A; A");
        backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/400ms);
    }
    track_fade_in(look, progress);
    if (opening_skip_due(look, progress)){
        backend.log("[PLZA] platform wait: the opening plays; Plus held to skip it");
        backend.press(BUTTON_PLUS, CUTSCENE_SKIP_HOLD, /*release=*/400ms);
        progress.faded_in.reset();
    }
}

struct OpeningOutcome{
    std::optional<StoryFailure> failure;
    Milliseconds remaining = 0ms;
};

//  Phase 1 of opening_to_platform: advance text and skip cutscenes until the
//  arrival autosave badge appears, then hand the remaining budget to
//  wait_for_platform() so Plus can never fire after autosave.
OpeningOutcome skip_opening_to_autosave(PlzaBackend& backend, Milliseconds timeout){
    OpeningProgress progress;
    const Pokemon::WaitOutcome outcome = Pokemon::poll_until(
        backend,
        AUTOSAVE_SHOWS,
        Pokemon::WaitRule{.poll = PLATFORM_POLL, .timeout = timeout},
        [&](const ScreenObservation& look){
            advance_opening(backend, look, progress);
            backend.wait(PLATFORM_POLL);
        }
    );
    if (const auto* failure = std::get_if<StoryFailure>(&outcome)){
        return {.failure = *failure};
    }
    if (const auto* matched = std::get_if<Pokemon::WaitMatched>(&outcome)){
        backend.log("[PLZA] platform wait: the autosave badge is on screen");
        backend.wait(PLATFORM_POLL);
        return {.remaining = matched->elapsed >= timeout ? 0ms : timeout - matched->elapsed};
    }
    return {.failure = StoryFailure{.code = "platform_not_seen", .detail = std::format(
        "No autosave badge seen within {} ms.", timeout.count()
    )}};
}

} // namespace


std::optional<StoryFailure> wait_for_platform(PlzaBackend& backend, Milliseconds timeout){
    SignalProbe signals;
    const Pokemon::WaitOutcome outcome = Pokemon::wait_for(
        backend,
        PLATFORM_HINT,
        Pokemon::WaitRule{.poll = PLATFORM_POLL, .timeout = timeout, .confirm_looks = PLATFORM_LOOKS},
        [&](const ScreenObservation& look){
            signals.look(look);
        }
    );
    if (const auto* failure = std::get_if<StoryFailure>(&outcome)){
        return *failure;
    }
    if (const auto* matched = std::get_if<Pokemon::WaitMatched>(&outcome)){
        backend.log(std::format("[PLZA] platform wait: on the platform after {} looks", matched->looks));
        backend.log_detail(signals.line("wait_for_platform", "platform", /*by=*/"hint"));
        return std::nullopt;
    }
    return StoryFailure{.code = "platform_not_seen", .detail = std::format(
        "No controls hint on {} looks in a row within {} ms.",
        PLATFORM_LOOKS, timeout.count()
    )};
}

std::optional<StoryFailure> opening_to_platform(PlzaBackend& backend, Milliseconds timeout){
    const OpeningOutcome opening = skip_opening_to_autosave(backend, timeout);
    if (opening.failure){
        return opening.failure;
    }
    return wait_for_platform(backend, opening.remaining);
}




} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
