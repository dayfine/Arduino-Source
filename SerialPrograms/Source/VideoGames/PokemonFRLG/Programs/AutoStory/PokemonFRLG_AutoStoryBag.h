/*  AutoStory Bag
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The bag as the story's item steps drive it: its pockets, the rows of
 *  items, and the actions on it, the console's or a test's.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBag_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBag_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The bag's pockets, in the order LEFT and RIGHT go through them.
enum class BagPocket{
    ITEMS,
    KEY_ITEMS,
    POKE_BALLS,
};

//  The pocket of an item the story uses from the bag: repel, super-repel,
//  max-repel, escape-rope (Items) or bicycle (Key Items); nothing for
//  another.
std::optional<BagPocket> usable_item_pocket(std::string_view item);

//  Whether `item` is a Repel of any kind.
bool is_repel(std::string_view item);

//  The row of `item` in `rows` (read_bag_pocket()'s, from the top), if it
//  has one left: a count of -1 (unread) counts as some.
std::optional<size_t> bag_row_of(const std::vector<BagRow>& rows, std::string_view item);

//  The bag, as use_item_from_bag() drives it: the console's, or a test's.
class ItemUseActions{
public:
    virtual ~ItemUseActions() = default;
    //  From the overworld, opens the bag on `pocket`, scrolled to its top.
    virtual bool open_pocket(BagPocket pocket) = 0;
    //  The open pocket's rows, from the top.
    virtual std::vector<BagRow> read_pocket() = 0;
    //  Moves to row `row`, USE, and through what the item says.
    virtual void use_row(size_t row) = 0;
    //  Moves to row `row` and REGISTERs it for SELECT.
    virtual void register_row(size_t row) = 0;
    //  Back to the overworld (nothing to do if the item closed the bag).
    virtual void close_bag() = 0;
};

//  How use_item_from_bag() ended.
enum class ItemUseEnd{
    USED,
    //  The pocket has none left (or the name isn't read).
    NONE_LEFT,
    //  The bag didn't open.
    NO_BAG,
    //  Not an item the story uses from the bag.
    UNKNOWN_ITEM,
};

//  "used", "none_left": the end's name in logs and failures.
std::string item_use_end_name(ItemUseEnd end);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
