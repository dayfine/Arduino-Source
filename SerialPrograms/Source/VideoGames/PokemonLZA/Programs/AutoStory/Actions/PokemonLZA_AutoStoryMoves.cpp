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
    for (size_t index = 0; index < move.legs.size(); index++){
        const StoryWalkLeg& leg = move.legs[index];
        if (pause != nullptr){
            pause->before_leg(story_walk_leg_label(index, move.legs.size(), leg));
            backend.scope().throw_if_cancelled();
        }
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
                "fly_to(): could not fly to {} (state {}). The fast travel menu is read in {}: "
                "check that the Game Language option matches the game.",
                get_location_item_from_enum(location).slug, static_cast<int>(state), language_data(language).name
            ),
            console
        );
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
