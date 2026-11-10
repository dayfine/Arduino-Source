/*  PokemonLZA AutoStory - Moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include <numbers>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_FastTravelNavigation.h"
#include "PokemonLZA_AutoStoryMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


double read_heading(ConsoleHandle& console, ProControllerContext& context){
    return get_facing_direction(console, context);
}

namespace{

//  Recenters the camera behind the player (L), then taps the stick
//  `degrees` clockwise of straight ahead, which turns the player that way
//  (and steps it a little). 150 ms is the tap the story was recorded with
//  (console B, 2026-11-08).
void turn_by(ProControllerContext& context, double degrees){
    const double radians = degrees * std::numbers::pi / 180.0;
    pbf_press_button(context, BUTTON_L, 100ms, 400ms);
    pbf_move_left_joystick(context, {std::sin(radians), std::cos(radians)}, 150ms, 300ms);
}

} // namespace

double face_heading(ConsoleHandle& console, ProControllerContext& context, double heading, double tolerance){
    double now = get_facing_direction(console, context);
    for (size_t attempt = 0; attempt < 4 && get_angle_between_facing_directions(now, heading) > tolerance; attempt++){
        turn_by(context, heading - now);
        context.wait_for_all_requests();
        now = get_facing_direction(console, context);
    }
    if (get_angle_between_facing_directions(now, heading) > tolerance){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("face_heading(): facing {:.0f} deg, wanted {:.0f} +/- {:.0f}.", now, heading, tolerance),
            console
        );
    }
    return now;
}

void run_story_move(ConsoleHandle& console, ProControllerContext& context, const StoryMove& move){
    if (move.heading){
        face_heading(console, context, *move.heading, 10.0);
    }
    for (const StoryWalkLeg& leg : move.legs){
        if (leg.turn != 0){
            //  The walk goes where the camera looks: recenter it after the turn.
            turn_by(context, leg.turn);
            pbf_press_button(context, BUTTON_L, 100ms, 400ms);
        }
        if (leg.walk > Milliseconds(0)){
            pbf_move_left_joystick(context, {0, 1}, leg.walk, 0ms);
        }
    }
    context.wait_for_all_requests();
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
