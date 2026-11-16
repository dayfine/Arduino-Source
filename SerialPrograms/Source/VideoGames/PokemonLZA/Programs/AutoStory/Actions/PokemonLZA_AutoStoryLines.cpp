/*  PokemonLZA AutoStory - Lines
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryLines.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


//  How often press_a_through_lines() looks.
constexpr Milliseconds LINES_POLL(250);

LinesPassed press_a_through_lines(PlzaBackend& backend, Milliseconds quiet, Milliseconds limit){
    LinesPassed passed;
    //  The looks bound a script whose time stands still.
    const size_t max_looks = (size_t)(limit / LINES_POLL) + 1;
    std::optional<WallClock> start;
    std::optional<WallClock> last_busy;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        start = start.value_or(look.when);
        last_busy = last_busy.value_or(look.when);
        const bool card = look.readings.contains(TUTORIAL_CARD_READING);
        const bool text = look.events.has(ScreenEvent::DIALOG_BOX) || look.readings.contains(ADVANCE_ARROW_READING);
        if (card || text){
            passed.card = passed.card || card;
            passed.text = passed.text || text;
            last_busy = look.when;
            backend.press(BUTTON_A, 160ms, 240ms);
        }else if (look.when - *last_busy >= quiet){
            return passed;
        }
        if (look.when - *start >= limit){
            break;
        }
        backend.wait(LINES_POLL);
    }
    passed.timed_out = true;
    return passed;
}

bool press_b_out_of_menu(PlzaBackend& backend, size_t presses){
    for (size_t pressed = 0; pressed < presses; pressed++){
        backend.scope().throw_if_cancelled();
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (look.events.has(ScreenEvent::PLAYER_CAN_MOVE) || look.events.has(ScreenEvent::DIALOG_BOX)){
            return true;
        }
        backend.press(BUTTON_B, 160ms, 840ms);
    }
    const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) || look.events.has(ScreenEvent::DIALOG_BOX);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
