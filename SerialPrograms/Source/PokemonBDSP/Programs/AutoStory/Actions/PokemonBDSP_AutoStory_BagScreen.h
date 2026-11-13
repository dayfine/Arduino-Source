/*  BDSP AutoStory - Bag screen
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which pocket and row the open Bag shows, and the presses between pockets.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_BagScreen_H
#define PokemonAutomation_PokemonBDSP_AutoStory_BagScreen_H

#include <cstddef>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Pocket positions 1-8: Medicine, Balls, Battle Items, Berries, Other Items,
//  TMs, Treasures, Key Items. Zero means the selected tab is uncertain.
int bag_pocket_position(const ImageViewRGB32& frame);
//  Whether the open pocket shows no item.
bool bag_list_is_empty(const ImageViewRGB32& frame);
//  The cursor's row, 1-7, or 0 if unknown.
int bag_cursor_position(const ImageViewRGB32& frame);

//  Navigation action between bag pocket tabs.
enum class BagPocketAction{
    UNKNOWN,
    CONFIRM,
    LEFT,
    RIGHT,
};
//  Plan to navigate from one bag pocket tab to another.
struct BagPocketMovePlan{
    BagPocketAction action = BagPocketAction::UNKNOWN;
    size_t presses = 0;
};
//  Circular eight-pocket route; ties prefer Right. Invalid positions send no input.
BagPocketMovePlan plan_bag_pocket_move(int current, int target);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
