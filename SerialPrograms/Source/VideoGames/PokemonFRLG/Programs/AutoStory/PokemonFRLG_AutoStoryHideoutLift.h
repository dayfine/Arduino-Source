/*  AutoStory Hideout Lift
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Riding the Rocket Hideout's lift: its panel's menu, the presses that
 *  pick a floor, and the ride on the console.
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
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The lift panel's menu, top first (pokefirered
//  sMultichoiceList_RocketHideoutElevator): B1F, B2F, B4F, EXIT.
const std::vector<std::string>& rocket_hideout_lift_menu();

//  A ride: the menu rows the cursor starts on and is moved to.
struct LiftRide{
    size_t from = 0;
    size_t to = 0;
};

//  The ride a lift_floor step names, or nothing if it is not one, or names
//  a floor the panel hasn't.
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
//  the floor picked, out onto the floor's lift door.
void ride_lift(ConsoleHandle& console, ProControllerContext& context, const LiftRide& ride);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
