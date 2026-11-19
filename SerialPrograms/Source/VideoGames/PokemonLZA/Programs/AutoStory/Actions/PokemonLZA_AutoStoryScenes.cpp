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
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryReadings.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

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
    return text_shows(look) && part == MapTalkPart::BEFORE_CARD ? BUTTON_A : BUTTON_B;
}


constexpr Milliseconds CHOICE_WAIT(3000);
constexpr Milliseconds CHOICE_POLL(250);

//  (highlighted row, row count).
std::optional<std::pair<int, int>> choice_rows(const ScreenObservation& look){
    const auto selected = look.readings.find(CHOICE_SELECTED_READING);
    const auto rows = look.readings.find(CHOICE_ROWS_READING);
    if (!look.events.has(ScreenEvent::CHOICE_BOX) || selected == look.readings.end() || rows == look.readings.end()){
        return std::nullopt;
    }
    return std::make_pair(selected->second, rows->second);
}

std::optional<std::pair<int, int>> wait_for_choice_rows(PlzaBackend& backend){
    for (Milliseconds waited(0); ; waited += CHOICE_POLL){
        backend.scope().throw_if_cancelled();
        const std::optional<std::pair<int, int>> rows = choice_rows(backend.observe());
        if (rows || waited >= CHOICE_WAIT){
            return rows;
        }
        backend.wait(CHOICE_POLL);
    }
}

//  A presses through a question's text (e.g. "Which floor?") before its choices show.
constexpr int CHOICE_TEXT_PRESSES = 3;

} // namespace


Pokemon::Outcome talk_through_map_screen(PlzaBackend& backend){
    //  Bounds the looks too, for a script whose time stands still.
    const size_t max_looks = (size_t)(MAP_TALK_TIMEOUT / MAP_TALK_POLL) + 1;
    std::optional<WallClock> start;
    std::optional<WallClock> control_since;
    MapTalkPart part = MapTalkPart::BEFORE_CARD;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        start = start.value_or(look.when);
        if (party_bar_free(look)){
            control_since = control_since.value_or(look.when);
            if (look.when - *control_since >= MAP_TALK_CONTROL_HOLD){
                backend.log("[PLZA] talk_through_map_screen: the party bar held: back in the overworld");
                return Pokemon::outcome_ok();
            }
            backend.wait(MAP_TALK_POLL);
            continue;
        }
        control_since.reset();
        if (look.when - *start >= MAP_TALK_TIMEOUT){
            break;
        }
        backend.press(map_talk_button(look, part), MAP_TALK_HOLD, MAP_TALK_RELEASE);
        if (look.readings.contains(TUTORIAL_CARD_READING)){
            part = MapTalkPart::AFTER_CARD;
        }
    }
    return Pokemon::outcome_failed("map_talk_timeout", std::format(
        "no party bar held {} ms within {} ms of A on the talk and the card, B on the map",
        MAP_TALK_CONTROL_HOLD.count(), MAP_TALK_TIMEOUT.count()
    ));
}

void climb_ladder(PlzaBackend& backend){
    backend.stick(/*degrees=*/90, /*duration=*/600ms, /*release=*/300ms, Pokemon::StoryPace::WALK);
    backend.stick(/*degrees=*/0, /*duration=*/150ms, /*release=*/300ms, Pokemon::StoryPace::WALK);
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/1000ms);
    backend.stick(/*degrees=*/0, /*duration=*/5000ms, /*release=*/500ms, Pokemon::StoryPace::WALK);
}

Pokemon::Outcome choose_row(PlzaBackend& backend, int index){
    std::optional<std::pair<int, int>> rows = wait_for_choice_rows(backend);
    for (int press = 0; !rows && press < CHOICE_TEXT_PRESSES; press++){
        backend.log(std::format("[PLZA] choose_{}: no choice yet: A through the text", index));
        backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
        rows = wait_for_choice_rows(backend);
    }
    if (!rows){
        return Pokemon::outcome_failed("no_choice", std::format("no choice box with rows that read after {} A presses", CHOICE_TEXT_PRESSES));
    }
    const auto [selected, count] = *rows;
    if (index >= count){
        return Pokemon::outcome_failed("no_such_choice", std::format("row {} of a choice of {} rows", index, count));
    }
    backend.log(std::format("[PLZA] choose_{}: row {} of {} highlighted", index, selected, count));
    const int moves = index - selected;
    for (int moved = 0; moved < std::abs(moves); moved++){
        backend.press(moves > 0 ? BUTTON_DOWN : BUTTON_UP, /*hold=*/160ms, /*release=*/240ms);
    }
    if (moves != 0){
        const std::optional<std::pair<int, int>> now = choice_rows(backend.observe());
        if (!now || now->first != index){
            return Pokemon::outcome_failed("choice_not_reached", std::format(
                "the highlight is on row {}, not {}, after {} presses from row {}",
                now ? std::to_string(now->first) : "none", index, std::abs(moves), selected
            ));
        }
    }
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
    return Pokemon::outcome_ok();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
