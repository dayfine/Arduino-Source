/*  PokemonLZA AutoStory - Moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_FastTravelNavigation.h"
#include "PokemonLZA_AutoStoryMoves.h"

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

Pokemon::Outcome run_story_move(PlzaBackend& backend, const StoryMove& move, const PlzaReader& heading){
    if (move.heading){
        const Pokemon::Outcome faced = face_heading(backend, heading, *move.heading, 10.0);
        if (!faced.succeeded()){
            return faced;
        }
    }
    if (move.wait_first > Milliseconds(0)){
        backend.wait(move.wait_first);
    }
    if (move.held){
        for (size_t i = 0; i < move.legs.size(); i++){
            const StoryWalkLeg& leg = move.legs[i];
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
    for (const StoryWalkLeg& leg : move.legs){
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


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
