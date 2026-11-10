/*  AutoStory Field Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Table constructors for the GAME steps on the field: the hideout's
 *  switch and lift, Pokemon Centers, and items from the bag.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFieldSteps_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFieldSteps_H

#include <optional>
#include <string>
#include <string_view>
#include "PokemonFRLG_AutoStoryTableSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace StoryTable{


//  Pushes the switch behind the Game Corner's poster the player faces
//  (POSTER_SWITCH_VARIANT).
StoryStep poster_switch(std::string name, std::string why);

//  Rides the Rocket Hideout's lift from the floor it stands at to `to`
//  (LIFT_FLOOR_VARIANT): "B1F", "B2F", "B4F". The player starts below
//  `from`'s lift door, facing north, and ends on `to`'s, facing south.
StoryStep lift_floor(std::string name, std::string_view from, std::string_view to, std::string why);

//  Into the Pokemon Center `center` (a map name) from below its door,
//  facing north, to below the nurse's counter (CENTER_ENTER_VARIANT): the
//  same script in every Center, with no map of its rooms.
StoryStep center_enter(std::string name, const std::string& center, std::string why);

//  Out of the Center from below the nurse's counter (CENTER_EXIT_VARIANT)
//  to `below_door`, the tile below its door outside, facing south.
StoryStep center_exit(std::string name, Pokemon::Pose below_door, std::string why);

//  Uses `item` from the bag on the overworld (USE_ITEM_VARIANT): a
//  Repel, or an Escape Rope, which leaves the player on `pose_after`.
StoryStep use_item(std::string name, std::string_view item, std::string why, std::optional<Pokemon::Pose> pose_after = std::nullopt);

//  Gets on the Bicycle (RIDE_BIKE_VARIANT).
StoryStep ride_bike(std::string name, std::string why);


} // namespace StoryTable
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
