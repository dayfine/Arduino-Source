/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenWait.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryReadings.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenPredicates.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryWalkScreenChecks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  Up the 03.03 ladder to the roof: about 6 s by hand on 2026-11-19.
constexpr Milliseconds LADDER_CLIMB = 6500ms;
constexpr Milliseconds MAP_TALK_TIMEOUT = 120s;
constexpr Milliseconds MAP_TALK_CONTROL_HOLD = 1000ms;
constexpr Milliseconds MAP_TALK_HOLD = 160ms;
constexpr Milliseconds MAP_TALK_RELEASE = 840ms;
constexpr Milliseconds MAP_TALK_POLL = 250ms;

//  A on a tutorial card (B turns its pages back) and on text before it; B
//  on the rest: on the map A would place a pin.
enum class MapTalkPart{BEFORE_CARD, AFTER_CARD};
Button map_talk_button(const ScreenObservation& look, MapTalkPart part){
    if (look.readings.contains(TUTORIAL_CARD_READING)){
        return BUTTON_A;
    }
    return TEXT_SHOWS.matches(look) && part == MapTalkPart::BEFORE_CARD ? BUTTON_A : BUTTON_B;
}


constexpr Milliseconds CHOICE_WAIT(3000);
constexpr Milliseconds CHOICE_POLL(250);

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

//  A presses through a question's text (e.g. "Which floor?") before its choices show.
constexpr int CHOICE_TEXT_PRESSES = 3;

} // namespace


std::optional<Pokemon::StoryFailure> talk_through_map_screen(PlzaBackend& backend){
    MapTalkPart part = MapTalkPart::BEFORE_CARD;
    const Pokemon::WaitOutcome outcome = Pokemon::poll_until(
        backend,
        PARTY_BAR_FREE,
        Pokemon::WaitRule{.poll = MAP_TALK_POLL, .timeout = MAP_TALK_TIMEOUT, .confirm_hold = MAP_TALK_CONTROL_HOLD},
        [&](const ScreenObservation& look){
            if (PARTY_BAR_FREE.matches(look)){
                backend.wait(MAP_TALK_POLL);
                return;
            }
            backend.press(map_talk_button(look, part), MAP_TALK_HOLD, MAP_TALK_RELEASE);
            if (look.readings.contains(TUTORIAL_CARD_READING)){
                part = MapTalkPart::AFTER_CARD;
            }
        }
    );
    if (const auto* failure = std::get_if<Pokemon::StoryFailure>(&outcome)){
        return *failure;
    }
    if (std::holds_alternative<Pokemon::WaitMatched>(outcome)){
        backend.log("[PLZA] talk_through_map_screen: the party bar held: back in the overworld");
        return std::nullopt;
    }
    return Pokemon::StoryFailure{.code = "map_talk_timeout", .detail = std::format(
        "no party bar held {} ms within {} ms of A on the talk and the card, B on the map",
        MAP_TALK_CONTROL_HOLD.count(), MAP_TALK_TIMEOUT.count()
    )};
}

std::optional<Pokemon::StoryFailure> climb_ladder(PlzaBackend& backend){
    //  The run-pace side street leg stops left or right of the ladder
    //  (2026-11-19): a fixed sidestep missed it, so step along the wall
    //  until the prompt shows. "Climb Up" shows only facing the ladder.
    backend.stick(/*degrees=*/0, /*duration=*/150ms, /*release=*/500ms, Pokemon::StoryPace::WALK);
    const std::optional<size_t> steps = step_along_until_prompt(backend, /*facing_degrees=*/0);
    if (!steps){
        return Pokemon::StoryFailure{.code = "ladder_not_found", .detail = "no Climb Up prompt stepping along the wall"};
    }
    backend.log(std::format("[PLZA] climb_ladder: Climb Up after {} steps along the wall", *steps));
    press_through_prompt(backend);
    backend.stick(/*degrees=*/0, /*duration=*/LADDER_CLIMB, /*release=*/500ms, Pokemon::StoryPace::WALK);
    return std::nullopt;
}

std::optional<Pokemon::StoryFailure> choose_row(PlzaBackend& backend, int index){
    Pokemon::WaitOutcome outcome = wait_for_choice_rows(backend);
    for (int press = 0; std::holds_alternative<Pokemon::WaitTimeout>(outcome) && press < CHOICE_TEXT_PRESSES; press++){
        backend.log(std::format("[PLZA] choose_{}: no choice yet: A through the text", index));
        backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
        outcome = wait_for_choice_rows(backend);
    }
    if (const auto* failure = std::get_if<Pokemon::StoryFailure>(&outcome)){
        return *failure;
    }
    const auto* matched = std::get_if<Pokemon::WaitMatched>(&outcome);
    if (matched == nullptr){
        return Pokemon::StoryFailure{.code = "no_choice", .detail = std::format("no choice box with rows that read after {} A presses", CHOICE_TEXT_PRESSES)};
    }
    const auto [selected, count] = *choice_rows(matched->look);
    if (index >= count){
        return Pokemon::StoryFailure{.code = "no_such_choice", .detail = std::format("row {} of a choice of {} rows", index, count)};
    }
    backend.log(std::format("[PLZA] choose_{}: row {} of {} highlighted", index, selected, count));
    const int moves = index - selected;
    for (int moved = 0; moved < std::abs(moves); moved++){
        backend.press(moves > 0 ? BUTTON_DOWN : BUTTON_UP, /*hold=*/160ms, /*release=*/240ms);
    }
    if (moves != 0){
        const std::optional<std::pair<int, int>> now = choice_rows(backend.observe());
        if (!now || now->first != index){
            return Pokemon::StoryFailure{.code = "choice_not_reached", .detail = std::format(
                "the highlight is on row {}, not {}, after {} presses from row {}",
                now ? std::to_string(now->first) : "none", index, std::abs(moves), selected
            )};
        }
    }
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
    return std::nullopt;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
