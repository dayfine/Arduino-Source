/*  PokemonLZA AutoStory - Lines
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryReadings.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySignalProbe.h"
#include "PokemonLZA_AutoStoryLines.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


constexpr Milliseconds LINES_POLL(250);

LinesPassed press_a_through_lines(PlzaBackend& backend, Milliseconds quiet, Milliseconds limit){
    LinesPassed passed;
    //  Bounds the looks too, for a script whose time stands still.
    const size_t max_looks = (size_t)(limit / LINES_POLL) + 1;
    std::optional<WallClock> start;
    std::optional<WallClock> last_busy;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        start = start.value_or(look.when);
        last_busy = last_busy.value_or(look.when);
        const bool card = look.readings.contains(TUTORIAL_CARD_READING);
        const bool text = text_shows(look);
        if (card || text){
            passed.card = passed.card || card;
            passed.text = passed.text || text;
            last_busy = look.when;
            backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
        }else if (look.when - *last_busy >= quiet){
            return passed;
        }
        if (look.when - *start >= limit){
            break;
        }
        //  The next look right after a press: the press's 400 ms is the beat.
        if (!card && !text){
            backend.wait(LINES_POLL);
        }
    }
    passed.timed_out = true;
    return passed;
}

namespace{

//  The party bar or a text box on `look`, logged as the exit's "[Signals]" line.
bool out_of_menu(const ScreenObservation& look, PlzaBackend& backend, SignalProbe& signals){
    signals.look(look);
    const bool party_bar = look.events.has(ScreenEvent::PLAYER_CAN_MOVE);
    if (!party_bar && !look.events.has(ScreenEvent::DIALOG_BOX)){
        return false;
    }
    backend.log_detail(signals.line("press_b_out_of_menu", "menu_exit", party_bar ? "party_bar" : "dialog_box"));
    return true;
}

} // namespace

bool press_b_out_of_menu(PlzaBackend& backend, size_t presses){
    SignalProbe signals;
    for (size_t pressed = 0; pressed < presses; pressed++){
        backend.scope().throw_if_cancelled();
        if (out_of_menu(backend.observe(), backend, signals)){
            return true;
        }
        backend.press(BUTTON_B, /*hold=*/160ms, /*release=*/840ms);
    }
    return out_of_menu(backend.observe(), backend, signals);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
