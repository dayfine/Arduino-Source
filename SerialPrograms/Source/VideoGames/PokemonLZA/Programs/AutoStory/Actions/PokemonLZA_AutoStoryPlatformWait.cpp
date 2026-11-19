/*  PokemonLZA AutoStory - Platform wait
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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

struct PlatformProgress{
    bool autosave_seen = false;
    size_t streak = 0;
    //  On the last look.
    bool text_waiting = false;
    bool fade_shown = false;
    //  When the picture came back after a fade, until Plus is held for it.
    std::optional<WallClock> faded_in = std::nullopt;
};

//  True once the platform's controls hint ("(L) Move") has held
//  PLATFORM_LOOKS looks: the platform has no party bar.
bool platform_look(PlzaBackend& backend, const ScreenObservation& look, PlatformWait wait, PlatformProgress& progress){
    if (look.readings.contains(AUTOSAVE_READING)){
        if (!progress.autosave_seen){
            backend.log("[PLZA] platform wait: the autosave badge is on screen");
        }
        progress.autosave_seen = true;
        progress.streak = 0;
        return false;
    }
    const bool counts = wait == PlatformWait::PLATFORM_ONLY || progress.autosave_seen;
    progress.streak = counts && look.readings.contains(CONTROLS_HINT_READING) ? progress.streak + 1 : 0;
    return progress.streak >= PLATFORM_LOOKS;
}

//  The opening's last line ("Your train has arrived in Lumiose City!") waits for A.
bool text_waiting_for_a(const ScreenObservation& look){
    return look.events.has(ScreenEvent::DIALOG_BOX) && look.readings.contains(ADVANCE_ARROW_READING);
}

//  A Plus held while the confirm screen still fades out does nothing (console
//  B, 2026-11-19: held 4 ms into the step). The opening's "hold to skip"
//  takes one this long after its picture fades in.
constexpr Milliseconds OPENING_SKIP_AFTER_FADE_IN = 750ms;

void track_fade_in(const ScreenObservation& look, PlatformProgress& progress){
    const bool fade = fade_shows(look);
    if (fade){
        progress.faded_in.reset();
    }else if (progress.fade_shown){
        progress.faded_in = look.when;
    }
    progress.fade_shown = fade;
}

//  Once per fade-in, while the opening plays. Plus on the platform opens the
//  map, so none once its controls hint or the autosave before it shows.
bool opening_skip_due(const ScreenObservation& look, PlatformWait wait, const PlatformProgress& progress){
    if (wait != PlatformWait::AFTER_AUTOSAVE || progress.autosave_seen || !progress.faded_in){
        return false;
    }
    return look.when - *progress.faded_in >= OPENING_SKIP_AFTER_FADE_IN && !text_shows(look) &&
        !look.readings.contains(CONTROLS_HINT_READING);
}

} // namespace


std::optional<StoryFailure> wait_for_platform(PlzaBackend& backend, Milliseconds timeout, PlatformWait wait){
    //  The looks bound a script whose time stands still.
    const size_t max_looks = (size_t)(timeout / PLATFORM_POLL) + 1;
    std::optional<WallClock> start;
    PlatformProgress progress;
    SignalProbe signals;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        signals.look(look);
        start = start.value_or(look.when);
        if (platform_look(backend, look, wait, progress)){
            backend.log(std::format("[PLZA] platform wait: on the platform after {} looks", looks + 1));
            backend.log_detail(signals.line("wait_for_platform", "platform", /*by=*/"hint"));
            return std::nullopt;
        }
        if (look.when - *start >= timeout){
            break;
        }
        const bool text_now = text_waiting_for_a(look);
        if (text_now && !progress.text_waiting){
            backend.log("[PLZA] platform wait: text waiting for A; A");
            backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/400ms);
        }
        progress.text_waiting = text_now;
        track_fade_in(look, progress);
        if (opening_skip_due(look, wait, progress)){
            backend.log("[PLZA] platform wait: the opening plays; Plus held to skip it");
            backend.press(BUTTON_PLUS, CUTSCENE_SKIP_HOLD, /*release=*/400ms);
            progress.faded_in.reset();
        }
        backend.wait(PLATFORM_POLL);
    }
    return StoryFailure{.code = "platform_not_seen", .detail = std::format(
        "No controls hint on {} looks in a row within {} ms (autosave {}).",
        PLATFORM_LOOKS, timeout.count(), progress.autosave_seen ? "seen" : "not seen"
    )};
}




} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
