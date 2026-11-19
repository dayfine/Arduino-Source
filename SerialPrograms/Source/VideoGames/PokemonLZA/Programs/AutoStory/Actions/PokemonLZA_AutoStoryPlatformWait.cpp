/*  PokemonLZA AutoStory - Platform wait
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryReadings.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
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
    bool skip_pressed = false;
};

//  True once the platform has held PLATFORM_LOOKS looks.
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
    progress.streak = counts && look.readings.contains(PLATFORM_READING) ? progress.streak + 1 : 0;
    return progress.streak >= PLATFORM_LOOKS;
}

//  The opening's last line ("Your train has arrived in Lumiose City!") waits for A.
bool text_waiting_for_a(const ScreenObservation& look){
    return look.events.has(ScreenEvent::DIALOG_BOX) && look.readings.contains(ADVANCE_ARROW_READING);
}

//  The opening playing, not yet skipped: Plus at its start skips it to the
//  platform (console B, 2026-11-19); one press, as Plus on the platform
//  would open the map.
bool opening_skip_due(const ScreenObservation& look, PlatformWait wait, const PlatformProgress& progress){
    if (wait != PlatformWait::AFTER_AUTOSAVE || progress.skip_pressed || progress.autosave_seen){
        return false;
    }
    return !text_shows(look) && !fade_shows(look) && !look.events.has(ScreenEvent::SCREEN_STILL) &&
        !look.readings.contains(PLATFORM_READING);
}

} // namespace


std::optional<StoryFailure> wait_for_platform(PlzaBackend& backend, Milliseconds timeout, PlatformWait wait){
    //  The looks bound a script whose time stands still.
    const size_t max_looks = (size_t)(timeout / PLATFORM_POLL) + 1;
    std::optional<WallClock> start;
    PlatformProgress progress;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        start = start.value_or(look.when);
        if (platform_look(backend, look, wait, progress)){
            backend.log(std::format("[PLZA] platform wait: on the platform after {} looks", looks + 1));
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
        if (opening_skip_due(look, wait, progress)){
            backend.log("[PLZA] platform wait: the opening plays; Plus held to skip it");
            backend.press(BUTTON_PLUS, CUTSCENE_SKIP_HOLD, /*release=*/400ms);
            progress.skip_pressed = true;
        }
        backend.wait(PLATFORM_POLL);
    }
    return StoryFailure{.code = "platform_not_seen", .detail = std::format(
        "No platform on {} looks in a row within {} ms (autosave {}).",
        PLATFORM_LOOKS, timeout.count(), progress.autosave_seen ? "seen" : "not seen"
    )};
}

bool platform_holds_within(PlzaBackend& backend, size_t looks){
    size_t streak = 0;
    for (size_t look = 0; look < looks; look++){
        backend.scope().throw_if_cancelled();
        streak = backend.observe().readings.contains(PLATFORM_READING) ? streak + 1 : 0;
        if (streak >= PLATFORM_LOOKS){
            return true;
        }
        backend.wait(PLATFORM_POLL);
    }
    return false;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
