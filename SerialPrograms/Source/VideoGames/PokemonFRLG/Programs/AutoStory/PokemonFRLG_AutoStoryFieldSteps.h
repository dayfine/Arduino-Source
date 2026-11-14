/*  AutoStory Field Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The field GAME steps (PokemonFRLG_AutoStoryHideout.h,
 *  PokemonFRLG_AutoStoryItemUse.h, PokemonFRLG_AutoStoryRooms.h) as a
 *  checkpoint's JSON gives them, for tests.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFieldSteps_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFieldSteps_H

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Pushes the switch behind the Game Corner's poster the player faces
//  (POSTER_SWITCH_VARIANT).
Pokemon::StoryStep poster_switch_step(std::string name);

//  Rides a lift from the floor it stands at to `to` (LIFT_FLOOR_VARIANT):
//  "B1F", "B2F", "B4F" (the Rocket Hideout's) or "1F" to "5F" (the
//  Department Store's). The player starts below `from`'s lift door,
//  facing north, and ends a tile below `to`'s, facing south (lift_exits()).
Pokemon::StoryStep lift_floor_step(std::string name, std::string_view from, std::string_view to);

//  Into the Pokemon Center `center` (a map name) from below its door,
//  facing north, to below the nurse's counter (CENTER_ENTER_VARIANT).
Pokemon::StoryStep center_enter_step(std::string name, std::string_view center);

//  Out of the Center from below the nurse's counter (CENTER_EXIT_VARIANT)
//  to `below_door`, the tile below its door outside, facing south.
Pokemon::StoryStep center_exit_step(std::string name, Pokemon::GridPose below_door);

//  Uses `item` from the bag on the overworld (USE_ITEM_VARIANT): a
//  Repel, or an Escape Rope, which leaves the player on `pose_after`.
Pokemon::StoryStep use_item_step(std::string name, std::string_view item, std::optional<Pokemon::GridPose> pose_after = std::nullopt);

//  Gets on the Bicycle (RIDE_BIKE_VARIANT).
Pokemon::StoryStep ride_bike_step(std::string name);

//  A GAME step written in one line, e.g. "lift_floor B2F B4F": its first
//  word the action, "key=value" words args, and the plain words after it
//  `positional`'s keys in order. Every arg is a word.
Pokemon::StoryStep game_step_of(std::string_view text, std::span<const std::string_view> positional = {});



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
