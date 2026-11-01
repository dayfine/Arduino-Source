/*  BDSP AutoStory - Bag
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 *  Finding an item in the Bag, and Potions on the lead.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Bag_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Bag_H

#include <cstddef>
#include <string>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

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

enum class BagPocketAction{
    UNKNOWN,
    CONFIRM,
    LEFT,
    RIGHT,
};
struct BagPocketMovePlan{
    BagPocketAction action = BagPocketAction::UNKNOWN;
    size_t presses = 0;
};
//  Circular eight-pocket route; ties prefer Right. Invalid positions send no input.
BagPocketMovePlan plan_bag_pocket_move(int current, int target);

//  Starts in the overworld, opens the Bag on `target_pocket`, and moves the cursor
//  onto the item whose name reads `name` (letters only, lower case, e.g. "potion").
//  Reads the cursor row's name with OCR, so it does not depend on the capture
//  card's colors or sharpness the way a template match does.
//  Returns false if the pocket is empty or the name is not found.
bool find_bag_item_by_name(
    VideoStream& stream,
    ProControllerContext& context,
    const std::string& name,
    int target_pocket,
    int num_icons = 8
);
//  The same search in a Bag that is already open, such as the one a battle's
//  Bag command opens.
bool find_bag_item_in_open_bag(
    VideoStream& stream,
    ProControllerContext& context,
    const std::string& name,
    int target_pocket
);

//  Open the Bag from the overworld, look for `name` in the pocket, and close
//  it again. True if the item is there.
bool bag_has_item(
    VideoStream& stream,
    ProControllerContext& context,
    const std::string& name,
    int target_pocket
);

//  Read the lead's HP bar in the Bag and, if it is below `heal_below` (a
//  fraction of full), use Potions until it is full. heal_below = 1 always
//  heals; 0.5 only checks and heals when the lead is under half, and is
//  skipped when the Bag has no Potion.
void use_potion_first_pokemon(
    VideoStream& stream,
    ProControllerContext& context,
    double heal_below = 1.0
);



}
}
}
#endif
