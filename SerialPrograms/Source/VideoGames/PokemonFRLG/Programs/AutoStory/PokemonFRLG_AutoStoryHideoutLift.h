/*  AutoStory Hideout Lift
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Riding a lift, the Rocket Hideout's or the Celadon Department Store's
 *  (one layout): its panel's menu, the presses that pick a floor, and the
 *  ride on the console.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryHideoutLift_H
#define PokemonAutomation_PokemonFRLG_AutoStoryHideoutLift_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_HideoutLiftRoom.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The panel's menu of the lift whose room is `lift`, top first; empty for
//  another map. LIFT_MAP: B1F, B2F, B4F, EXIT (pokefirered
//  sMultichoiceList_RocketHideoutElevator); DEPT_STORE_LIFT_MAP: 5F, 4F,
//  3F, 2F, 1F, EXIT (sMultichoiceList_DeptStoreElevator).
const std::vector<std::string>& lift_menu(std::string_view lift);
//  Where the lift `lift` lets out for each floor on its panel, top first;
//  empty for another map.
const std::vector<MapPosition>& lift_exits(std::string_view lift);

//  A ride: the lift's room, and the menu rows the cursor starts on (the
//  floor the player came from: pokefirered GetElevatorFloor()) and is
//  moved to.
struct LiftRide{
    size_t from = 0;
    size_t to = 0;
    std::string_view lift = LIFT_MAP;
};

//  The ride a lift_floor step names, or nothing if it is not one, or names
//  floors no one panel has. The floors' names pick the lift: B1F..B4F the
//  hideout's, 1F..5F the store's.
std::optional<LiftRide> parse_lift_floor_step(const Pokemon::StoryStep& step);

//  Presses of the D-pad on a menu: which way, how many.
struct MenuPresses{
    Pokemon::Direction direction = Pokemon::Direction::SOUTH;
    size_t presses = 0;
};

//  The fewest presses that move the cursor from `ride.from` to `ride.to`
//  on a menu of `entries` rows. A multichoice of more than three rows
//  wraps round (pokefirered DrawVerticalMultichoiceMenu()).
MenuPresses lift_menu_presses(const LiftRide& ride, size_t entries);

//  On the console: A on the panel, the floor picked, the ride waited out
//  back to the overworld, the player still in the lift.
void pick_lift_floor(ConsoleHandle& console, ProControllerContext& context, const LiftRide& ride);

//  On the console, the whole ride: into the lift from below the door,
//  the floor picked, out a tile south of the floor's lift door.
void ride_lift(ConsoleHandle& console, ProControllerContext& context, const LiftRide& ride);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
