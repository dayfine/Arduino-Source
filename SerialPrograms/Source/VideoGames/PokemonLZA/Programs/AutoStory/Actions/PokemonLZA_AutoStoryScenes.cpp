/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  How long the map talk may take, and how long the party bar must hold.
constexpr Milliseconds MAP_TALK_TIMEOUT = 120s;
constexpr Milliseconds MAP_TALK_CONTROL_HOLD = 1000ms;
//  Each button's press and the wait after it.
constexpr Milliseconds MAP_TALK_HOLD = 160ms;
constexpr Milliseconds MAP_TALK_RELEASE = 840ms;
//  How often it looks while the party bar holds.
constexpr Milliseconds MAP_TALK_POLL = 250ms;

//  Free control: the party bar, with no text box over it (it shows between
//  the lines of a talk).
bool shows_control(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) && !look.events.has(ScreenEvent::DIALOG_BOX);
}

//  The button for what shows: A on a tutorial card, whose pages B turns
//  back, and on text until the card has shown; B on the rest: the map
//  app, where A places a pin, and her talk after it (as played by hand,
//  03.02b step 45).
enum class MapTalkPart{BEFORE_CARD, AFTER_CARD};
Button map_talk_button(const ScreenObservation& look, MapTalkPart part){
    if (look.readings.contains(TUTORIAL_CARD_READING)){
        return BUTTON_A;
    }
    const bool text = look.events.has(ScreenEvent::DIALOG_BOX) || look.readings.contains(ADVANCE_ARROW_READING);
    return text && part == MapTalkPart::BEFORE_CARD ? BUTTON_A : BUTTON_B;
}

} // namespace


Pokemon::Outcome talk_through_map_screen(PlzaBackend& backend){
    //  Each look presses for 1 s or waits; the looks bound a script whose
    //  time stands still.
    const size_t max_looks = (size_t)(MAP_TALK_TIMEOUT / MAP_TALK_POLL) + 1;
    std::optional<WallClock> start;
    std::optional<WallClock> control_since;
    MapTalkPart part = MapTalkPart::BEFORE_CARD;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        start = start.value_or(look.when);
        if (shows_control(look)){
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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
