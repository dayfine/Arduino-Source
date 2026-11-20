/*  PokemonLZA AutoStory - Lines
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenTimers.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenPredicates.h"
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
    Pokemon::HoldTimer quiet_hold(quiet);
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            passed.timed_out = true;
            return passed;
        }
        start = start.value_or(look.when);
        const bool card = look.readings.contains(TUTORIAL_CARD_READING);
        const bool text = TEXT_SHOWS.matches(look);
        if (card || text){
            passed.card = passed.card || card;
            passed.text = passed.text || text;
            quiet_hold.reset();
            backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
        }else if (quiet_hold.update(true, look.when)){
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
        const ScreenObservation look = backend.observe();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return false;
        }
        if (out_of_menu(look, backend, signals)){
            return true;
        }
        backend.press(BUTTON_B, /*hold=*/160ms, /*release=*/840ms);
    }
    const ScreenObservation look = backend.observe();
    if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
        return false;
    }
    return out_of_menu(look, backend, signals);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
