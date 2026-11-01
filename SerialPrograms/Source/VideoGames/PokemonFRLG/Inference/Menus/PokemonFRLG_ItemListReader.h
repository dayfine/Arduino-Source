/*  Item List Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads the item lists of the bag's pockets and of a Mart's BUY menu,
 *  which the game draws alike: rows 16 pixels apart from y 13, names from
 *  x 97 (pokefirered: the list window at tile (11, 1), item_X 9). It knows
 *  the names of the items a new game meets before the Boulder Badge, in
 *  English only so far: in another language a name is never read.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_ItemListReader_H
#define PokemonAutomation_PokemonFRLG_ItemListReader_H

#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Rows a list shows at once.
constexpr int ITEM_LIST_ROWS = 6;

//  Where row `row` (from 0) of a list shows its item's name, and in the bag
//  its count, as fractions of the game screen.
ImageFloatBox item_list_name_box(int row);
ImageFloatBox bag_item_count_box(int row);

//  The item named in row `row`: a slug such as "potion", "antidote",
//  "parlyz-heal", "poke-ball", or "cancel" for the list's last row, in a
//  game in `language`; empty if it can't be read, as in a language the
//  reader has no names for.
std::string read_item_list_name(Logger& logger, Language language, const ImageViewRGB32& screen, int row);

struct BagRow{
    std::string item;
    int count = 0;
};

//  The rows of the bag's open pocket from the top, up to its CANCEL (not
//  included). Stops at a row it can't read, so it is empty in a language
//  the reader has no names for. The pocket must be scrolled to its top.
std::vector<BagRow> read_bag_pocket(Logger& logger, Language language, const ImageViewRGB32& screen);



}
}
}
#endif
