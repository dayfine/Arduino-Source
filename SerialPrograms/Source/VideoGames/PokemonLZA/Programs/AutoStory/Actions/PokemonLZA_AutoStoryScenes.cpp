/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenWait.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenPredicates.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::StoryFailure;

namespace{

constexpr Milliseconds CHOICE_WAIT(3000);
constexpr Milliseconds CHOICE_POLL(250);
//  A presses through a question's text before its choices show.
constexpr int CHOICE_TEXT_PRESSES = 3;

//  (highlighted row, row count).
std::optional<std::pair<int, int>> choice_rows(const ScreenObservation& look){
    if (!CHOICE_WITH_ROWS.matches(look)){
        return std::nullopt;
    }
    return std::make_pair(
        look.readings.find(CHOICE_SELECTED_READING)->second,
        look.readings.find(CHOICE_ROWS_READING)->second
    );
}

Pokemon::WaitOutcome wait_for_choice_rows(PlzaBackend& backend){
    return Pokemon::wait_for(
        backend,
        CHOICE_WITH_ROWS,
        Pokemon::WaitRule{.poll = CHOICE_POLL, .timeout = CHOICE_WAIT}
    );
}

//  The autosave badge shows for 1-3 s: at least 2 looks a second.
constexpr Milliseconds PLATFORM_POLL(300);
constexpr size_t PLATFORM_LOOKS = 2;

constexpr Milliseconds TEXT_SPEED_POLL = 250ms;
constexpr Milliseconds MENU_OPEN_WAIT = 2000ms;
constexpr Milliseconds TEXT_SPEED_LIMIT = 30000ms;
constexpr size_t TEXT_SPEED_HELD_LOOKS = 2;

//  From the main menu to the options' text speed, set to Fast; each with
//  its release.
constexpr std::pair<Button, Milliseconds> TEXT_SPEED_PRESSES[] = {
    {BUTTON_L, 600ms},
    {BUTTON_UP, 300ms},
    {BUTTON_A, 1100ms},
    {BUTTON_DOWN, 300ms},
    {BUTTON_RIGHT, 300ms},
};

//  The save file's first menu opens under a tutorial card. A closes it; the
//  card fades out over ~1 s and reads as a card meanwhile, so a second A
//  too soon lands on the main menu and opens the bag (console B,
//  2026-11-19: 01.01 failed 3/3 that way).
constexpr Milliseconds CARD_CLOSE_WAIT = 1500ms;
constexpr size_t CARD_PAGES_MAX = 3;

//  B until the overworld holds; B on the overworld does nothing.
std::optional<StoryFailure> back_to_overworld(PlzaBackend& backend){
    const Pokemon::WaitOutcome outcome = Pokemon::poll_until(
        backend,
        OVERWORLD_FREE,
        Pokemon::WaitRule{.poll = TEXT_SPEED_POLL, .timeout = TEXT_SPEED_LIMIT, .confirm_looks = TEXT_SPEED_HELD_LOOKS},
        [&](const ScreenObservation& look){
            if (OVERWORLD_FREE.matches(look)){
                backend.wait(TEXT_SPEED_POLL);
            }else{
                backend.press(BUTTON_B, /*hold=*/160ms, /*release=*/840ms);
            }
        }
    );
    if (const auto* failure = std::get_if<StoryFailure>(&outcome)){
        return *failure;
    }
    if (const auto* matched = std::get_if<Pokemon::WaitMatched>(&outcome)){
        backend.log(std::format("[PLZA set_text_speed_fast] text speed set, back on the overworld at {:.1f}s", matched->elapsed.count() / 1000.0));
        return std::nullopt;
    }
    return StoryFailure{.code = "text_speed_timeout", .detail = std::format(
        "not back on the overworld in {} ms of B after the text speed rows", TEXT_SPEED_LIMIT.count()
    )};
}

} // namespace


std::optional<StoryFailure> choose_row(PlzaBackend& backend, int index){
    Pokemon::WaitOutcome outcome = wait_for_choice_rows(backend);
    for (int press = 0; std::holds_alternative<Pokemon::WaitTimeout>(outcome) && press < CHOICE_TEXT_PRESSES; press++){
        backend.log(std::format("[PLZA] choose_{}: no choice yet: A through the text", index));
        backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
        outcome = wait_for_choice_rows(backend);
    }
    if (const auto* failure = std::get_if<StoryFailure>(&outcome)){
        return *failure;
    }
    const auto* matched = std::get_if<Pokemon::WaitMatched>(&outcome);
    if (matched == nullptr){
        return StoryFailure{.code = "no_choice", .detail = std::format("no choice box with rows that read after {} A presses", CHOICE_TEXT_PRESSES)};
    }
    const auto [selected, count] = *choice_rows(matched->look);
    if (index >= count){
        return StoryFailure{.code = "no_such_choice", .detail = std::format("row {} of a choice of {} rows", index, count)};
    }
    backend.log(std::format("[PLZA] choose_{}: row {} of {} highlighted", index, selected, count));
    const int moves = index - selected;
    for (int moved = 0; moved < std::abs(moves); moved++){
        backend.press(moves > 0 ? BUTTON_DOWN : BUTTON_UP, /*hold=*/160ms, /*release=*/240ms);
    }
    if (moves != 0){
        const std::optional<std::pair<int, int>> now = choice_rows(backend.observe());
        if (!now || now->first != index){
            return StoryFailure{.code = "choice_not_reached", .detail = std::format(
                "the highlight is on row {}, not {}, after {} presses from row {}",
                now ? std::to_string(now->first) : "none", index, std::abs(moves), selected
            )};
        }
    }
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
    return std::nullopt;
}

std::optional<StoryFailure> wait_for_platform(PlzaBackend& backend, Milliseconds timeout){
    const Pokemon::WaitOutcome outcome = Pokemon::wait_for(
        backend,
        PLATFORM_HINT,
        Pokemon::WaitRule{.poll = PLATFORM_POLL, .timeout = timeout, .confirm_looks = PLATFORM_LOOKS}
    );
    if (const auto* failure = std::get_if<StoryFailure>(&outcome)){
        return *failure;
    }
    if (const auto* matched = std::get_if<Pokemon::WaitMatched>(&outcome)){
        backend.log(std::format("[PLZA] platform wait: on the platform after {} looks", matched->looks));
        return std::nullopt;
    }
    return StoryFailure{.code = "platform_not_seen", .detail = std::format(
        "No controls hint on {} looks in a row within {} ms.",
        PLATFORM_LOOKS, timeout.count()
    )};
}

//  Blind but for the first menu's tutorial card and the way back.
std::optional<StoryFailure> set_text_speed_fast(PlzaBackend& backend){
    backend.press(BUTTON_X, /*hold=*/160ms, /*release=*/240ms);
    backend.wait(MENU_OPEN_WAIT);
    for (size_t pages = 0; pages < CARD_PAGES_MAX; pages++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return Pokemon::game_not_running_failure();
        }
        if (!look.readings.contains(TUTORIAL_CARD_READING)){
            break;
        }
        backend.log("[PLZA set_text_speed_fast] tutorial card: A");
        backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
        backend.wait(CARD_CLOSE_WAIT);
    }
    for (const auto& [button, release] : TEXT_SPEED_PRESSES){
        backend.press(button, /*hold=*/100ms, release);
    }
    return back_to_overworld(backend);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
