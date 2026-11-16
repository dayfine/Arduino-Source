/*  AutoStory Pocket List
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A bag pocket's list read whole, scrolling below the six rows it shows:
 *  on the console or a test's list.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPocketList_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPocketList_H

#include <cstddef>
#include <optional>
#include <vector>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  An open pocket's list, as read_whole_pocket() scrolls it: the console's
//  or a test's. It shows ITEM_LIST_ROWS rows; DOWN moves the cursor to the
//  fourth row shown, then scrolls the list under it until its CANCEL shows
//  (item_shown_row()).
class PocketList{
public:
    virtual ~PocketList() = default;
    //  The row shown `shown`-th (from 0) now; nothing if the screen can't
    //  be read.
    virtual std::optional<BagRow> read_shown(int shown) = 0;
    //  DOWN: the cursor one row down.
    virtual void cursor_down() = 0;
    //  UP: the cursor one row up.
    virtual void cursor_up() = 0;
};

//  A pocket read whole, and the DOWN presses it took (and the UP presses
//  that put the cursor back on the top row after).
struct PocketRead{
    std::vector<BagRow> rows;
    size_t downs = 0;
};

//  The pocket's rows from the top, as pocket_rows() keeps them: the six
//  shown, then DOWN, a row at a time, until the CANCEL shows at the bottom
//  (or the list stops scrolling, its CANCEL unread). The cursor must start
//  on the top row. Reading the first six only ended 14.03's Rare Candy
//  step with "no rare-candy left", its RARE CANDY the 7th row (console B,
//  2026-11-14). It ends with the cursor back on the top row: the game
//  keeps a pocket's cursor between visits, the field's and the battle's
//  alike, and a read left on row 14 of 16 put the battle's 12 UP one row
//  short, so the X Special's DOWN presses stopped on the X Speed (console
//  B, 2026-11-16, 19.01b's rival fought with no setup).
PocketRead read_whole_pocket(PocketList& list);

//  The bag's open pocket on the console, read whole (read_whole_pocket()),
//  its cursor on the top row to start and to end.
PocketRead read_open_pocket(ConsoleHandle& console, ProControllerContext& context, Language language);

//  From the battle menu: BAG, its cursor on the Items pocket's top row. The
//  bag opens on the pocket used last; Items is the first. `rows` is the
//  pocket's rows as far as known, its CANCEL included: the UP presses
//  that bring the cursor to the top from wherever the last visit left it.
void open_battle_bag(ProControllerContext& context, size_t rows);
//  From the battle menu: the Items pocket through BAG (`rows` as
//  open_battle_bag() takes it), read whole, and back to the battle menu.
//  Empty if nothing could be read.
std::vector<BagRow> read_battle_pocket(ConsoleHandle& console, ProControllerContext& context, Language language, size_t rows);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
