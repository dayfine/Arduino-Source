/*  PokemonLZA AutoStory - Moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_FastTravelNavigation.h"
#include "PokemonLZA_AutoStoryMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


Pokemon::Outcome face_heading(PlzaStoryBackend& backend, double heading, double tolerance){
    double now = backend.heading();
    for (size_t attempt = 0; attempt < 4 && get_angle_between_facing_directions(now, heading) > tolerance; attempt++){
        backend.turn(heading - now);
        now = backend.heading();
    }
    if (get_angle_between_facing_directions(now, heading) > tolerance){
        return Pokemon::outcome_failed(
            "heading_missed",
            std::format("facing {:.0f} deg, wanted {:.0f} +/- {:.0f}", now, heading, tolerance)
        );
    }
    return Pokemon::outcome_ok();
}

Pokemon::Outcome run_story_move(PlzaStoryBackend& backend, const StoryMove& move){
    if (move.heading){
        const Pokemon::Outcome faced = face_heading(backend, *move.heading, 10.0);
        if (!faced.succeeded()){
            return faced;
        }
    }
    for (const StoryWalkLeg& leg : move.legs){
        if (leg.turn != 0){
            //  The walk goes where the camera looks: recenter it after the turn.
            backend.turn(leg.turn);
            backend.press(BUTTON_L, 100ms, 400ms);
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
