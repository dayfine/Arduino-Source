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
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Rows a list shows at once.
constexpr int ITEM_LIST_ROWS = 6;

//  Where row `row` (from 0) of a list shows its item's name, as fractions
//  of the game screen.
ImageFloatBox item_list_name_box(int row);

//  The item named in row `row`: a slug such as "potion", "antidote",
//  "parlyz-heal", "awakening", "poke-ball", "repel",
//  "super-repel", "max-repel", "escape-rope", "bicycle", or "cancel" for the list's last row, in a
//  game in `language`; empty if it can't be read, as in a language the
//  reader has no names for.
std::string read_item_list_name(Logger& logger, Language language, const ImageViewRGB32& screen, int row);

//  A single item entry in the bag pocket.
struct BagRow{
    //  The item's slug, as read_item_list_name() gives it; empty if its name
    //  can't be read.
    std::string item;
    //  -1 if the count couldn't be read: the row still holds at least one.
    int count = 0;
};

//  The item and count shown in row `shown` (from 0) of the bag's open
//  pocket; an empty item if its name can't be read.
BagRow read_bag_row(Logger& logger, Language language, const ImageViewRGB32& screen, int shown);

//  The rows of the bag's open pocket from the top, as pocket_rows() keeps
//  them. The pocket must be scrolled to its top.
std::vector<BagRow> read_bag_pocket(Logger& logger, Language language, const ImageViewRGB32& screen);

//  The rows of a pocket from `reads`, each row as read from the top: up to
//  the CANCEL (not included). A row whose name can't be read stays, with an
//  empty item, so the rows below keep their places: stopping there read
//  nothing below a MOON STONE, and the rival was fought with 0 of the bag's
//  15 Potions (console B, 2026-11-03). Unread rows after the last name read
//  are dropped. Empty if no name is read, as in a language the reader has
//  no names for.
std::vector<BagRow> pocket_rows(std::vector<BagRow> reads);


//  Registers item list reader unit tests.
void add_tests_ItemListReader(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
