/*  PokemonLZA AutoStory - Moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <limits>
#include <optional>
#include <string>
#include "Common/Cpp/Color.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_FastTravelNavigation.h"
#include "PokemonLZA_AutoStoryCatches.h"
#include "PokemonLZA_AutoStoryDialogs.h"
#include "PokemonLZA_AutoStoryMoves.h"
#include "PokemonLZA_AutoStoryZone1Road.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  The heading on a fresh look, read by `reader` when there is one.
std::optional<int> read_heading(PlzaBackend& backend, const PlzaReader& reader){
    Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
    if (reader){
        reader(backend, look);
    }
    return Pokemon::story_heading(look);
}

//  Leg `index` of `move` is about to start: a breakpoint, when `pause` is
//  given (StoryPausePoints::before_leg()).
void pause_before_leg(PlzaBackend& backend, Pokemon::StoryPausePoints* pause, const StoryMove& move, size_t index){
    if (pause == nullptr){
        return;
    }
    pause->before_leg(story_walk_leg_label(index, move.legs.size(), move.legs[index]));
    backend.scope().throw_if_cancelled();
}

} // namespace


Pokemon::Outcome face_heading(PlzaBackend& backend, const PlzaReader& reader, double heading, double tolerance){
    std::optional<int> now = read_heading(backend, reader);
    for (size_t attempt = 0; attempt < 4 && now && get_angle_between_facing_directions(*now, heading) > tolerance; attempt++){
        story_turn(backend, heading - *now);
        now = read_heading(backend, reader);
    }
    if (!now){
        return Pokemon::outcome_failed("heading_unread", "no look read the minimap's heading");
    }
    if (get_angle_between_facing_directions(*now, heading) > tolerance){
        return Pokemon::outcome_failed(
            "heading_missed",
            std::format("facing {} deg, wanted {:.0f} +/- {:.0f}", *now, heading, tolerance)
        );
    }
    return Pokemon::outcome_ok();
}

std::string story_walk_leg_label(size_t index, size_t count, const StoryWalkLeg& leg){
    std::string label = std::format("leg {}/{} turn {:g}", index + 1, count, leg.turn);
    if (leg.walk > Milliseconds(0)){
        label += std::format(" walk {} ms", leg.walk.count());
    }
    return label;
}

Pokemon::Outcome run_story_move(
    PlzaBackend& backend, const StoryMove& move, const PlzaReader& heading,
    Pokemon::StoryPausePoints* pause
){
    if (move.heading){
        const Pokemon::Outcome faced = face_heading(backend, heading, *move.heading, 10.0);
        if (!faced.succeeded()){
            return faced;
        }
    }
    if (move.held){
        for (size_t i = 0; i < move.legs.size(); i++){
            const StoryWalkLeg& leg = move.legs[i];
            pause_before_leg(backend, pause, move, i);
            if (i > 0 && leg.turn != 0){
                backend.press(BUTTON_L, 100ms, 150ms);
            }
            if (leg.walk > Milliseconds(0)){
                backend.stick(leg.turn, leg.walk, 0ms, Pokemon::StoryPace::WALK);
            }else if (leg.turn != 0){
                backend.stick(leg.turn, 150ms, 100ms, Pokemon::StoryPace::WALK);
            }
        }
        return Pokemon::outcome_ok();
    }
    for (size_t index = 0; index < move.legs.size(); index++){
        const StoryWalkLeg& leg = move.legs[index];
        pause_before_leg(backend, pause, move, index);
        if (leg.turn != 0){
            //  The walk goes where the camera looks: recenter it after the turn.
            story_turn(backend, leg.turn);
            backend.press(BUTTON_L, 100ms, 150ms);
        }
        if (leg.walk > Milliseconds(0)){
            backend.hold(Pokemon::Direction::NORTH, leg.walk, Pokemon::StoryPace::WALK);
        }
    }
    return Pokemon::outcome_ok();
}


void fly_to(ConsoleHandle& console, ProControllerContext& context, Language language, Location location){
    const FastTravelState state = open_map_and_fly_to(console, context, language, location);
    if (state != FastTravelState::SUCCESS){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format(
                "fly_to(): could not fly to {} (state {}).",
                get_location_item_from_enum(location).slug, static_cast<int>(state)
            ),
            console
        );
    }
}

bool fly_to_or_stay(ConsoleHandle& console, ProControllerContext& context, Language language, Location location){
    const std::string& slug = get_location_item_from_enum(location).slug;
    try{
        const FastTravelState state = open_map_and_fly_to(console, context, language, location);
        if (state == FastTravelState::SUCCESS){
            return true;
        }
        console.log(std::format("fly_to_or_stay(): no fly to {} (state {}): staying.", slug, static_cast<int>(state)), COLOR_RED);
    }catch (OperationFailedException& e){
        console.log(std::format("fly_to_or_stay(): no fly to {} ({}): staying.", slug, e.message()), COLOR_RED);
    }
    map_to_overworld(console, context);
    return false;
}

void center_to_zone_1(ConsoleHandle& console, ProControllerContext& context){
    //  Any distance ends the walk at once: this only finds the marker
    //  (turning the camera to it when it is behind) and reads it.
    const std::optional<int> start = walk_to_objective(console, context, std::numeric_limits<int>::max(), 20s);
    if (starts_in_zone_1(start)){
        console.log(std::format("center_to_zone_1(): {} from the far gate: in Wild Zone 1 already.", start.value_or(-1)));
        return;
    }
    console.log(std::format("center_to_zone_1(): {} from the far gate: to the zone's \"A Enter\".", start.value_or(-1)));
    walk_to_marker(console, context);
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    mash_to_overworld(console, context, BUTTON_B, 60s);
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
