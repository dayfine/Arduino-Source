/*  PokemonLZA AutoStory - Platform wait
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryReadings.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
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

Milliseconds elapsed_since(WallClock start, WallClock now){
    if (now <= start){
        return 0ms;
    }
    return std::chrono::duration_cast<Milliseconds>(now - start);
}

//  Bounds a scripted test whose observation timestamps stand still.
size_t max_poll_looks(Milliseconds timeout){
    if (timeout <= 0ms){
        return 1;
    }
    return (size_t)(timeout / PLATFORM_POLL) + 1;
}

//  The platform has no party bar; its controls hint ("(L) Move") stands in,
//  provided no autosave badge or dialog box covers it.
bool controls_hint_free(const ScreenObservation& look){
    return look.readings.contains(CONTROLS_HINT_READING) &&
        !look.readings.contains(AUTOSAVE_READING) &&
        !look.events.has(ScreenEvent::DIALOG_BOX);
}

struct OpeningProgress{
    //  On the last look.
    bool text_waiting = false;
    bool fade_shown = false;
    //  When the picture came back after a fade, until Plus is held for it.
    std::optional<WallClock> faded_in = std::nullopt;
};

//  The opening's last line ("Your train has arrived in Lumiose City!") waits for A.
bool text_waiting_for_a(const ScreenObservation& look){
    return look.events.has(ScreenEvent::DIALOG_BOX) && look.readings.contains(ADVANCE_ARROW_READING);
}

void track_fade_in(const ScreenObservation& look, OpeningProgress& progress){
    const bool fade = fade_shows(look);
    if (fade){
        progress.faded_in.reset();
    }else if (progress.fade_shown){
        progress.faded_in = look.when;
    }
    progress.fade_shown = fade;
}

//  Once per fade-in, while the opening plays. Plus on the platform opens the
//  map, so none once its controls hint shows (and this phase ends on autosave).
bool opening_skip_due(const ScreenObservation& look, const OpeningProgress& progress){
    if (!progress.faded_in){
        return false;
    }
    return elapsed_since(*progress.faded_in, look.when) >= OPENING_SKIP_AFTER_FADE_IN &&
        !text_shows(look) &&
        !look.readings.contains(CONTROLS_HINT_READING);
}

void advance_opening(PlzaBackend& backend, const ScreenObservation& look, OpeningProgress& progress){
    const bool text_now = text_waiting_for_a(look);
    if (text_now && !progress.text_waiting){
        backend.log("[PLZA] platform wait: text waiting for A; A");
        backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/400ms);
    }
    progress.text_waiting = text_now;
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
    const size_t max_looks = max_poll_looks(timeout);
    std::optional<WallClock> start;
    OpeningProgress progress;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return {.failure = Pokemon::game_not_running_failure()};
        }
        start = start.value_or(look.when);
        const Milliseconds elapsed = elapsed_since(*start, look.when);
        if (look.readings.contains(AUTOSAVE_READING)){
            backend.log("[PLZA] platform wait: the autosave badge is on screen");
            backend.wait(PLATFORM_POLL);
            return {.remaining = elapsed >= timeout ? 0ms : timeout - elapsed};
        }
        if (elapsed >= timeout){
            break;
        }
        advance_opening(backend, look, progress);
        backend.wait(PLATFORM_POLL);
    }
    return {.failure = StoryFailure{.code = "platform_not_seen", .detail = std::format(
        "No autosave badge seen within {} ms.", timeout.count()
    )}};
}

} // namespace


std::optional<StoryFailure> wait_for_platform(PlzaBackend& backend, Milliseconds timeout){
    const size_t max_looks = max_poll_looks(timeout);
    std::optional<WallClock> start;
    size_t streak = 0;
    SignalProbe signals;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        signals.look(look);
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return Pokemon::game_not_running_failure();
        }
        start = start.value_or(look.when);
        streak = controls_hint_free(look) ? streak + 1 : 0;
        if (streak >= PLATFORM_LOOKS){
            backend.log(std::format("[PLZA] platform wait: on the platform after {} looks", looks + 1));
            backend.log_detail(signals.line("wait_for_platform", "platform", /*by=*/"hint"));
            return std::nullopt;
        }
        if (elapsed_since(*start, look.when) >= timeout){
            break;
        }
        backend.wait(PLATFORM_POLL);
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
