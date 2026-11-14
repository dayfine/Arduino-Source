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
};

//  A pocket read whole, and the DOWN presses it took: as many UP put the
//  cursor back on the top row.
struct PocketRead{
    std::vector<BagRow> rows;
    size_t downs = 0;
};

//  The pocket's rows from the top, as pocket_rows() keeps them: the six
//  shown, then DOWN, a row at a time, until the CANCEL shows at the bottom
//  (or the list stops scrolling, its CANCEL unread). The cursor must start
//  on the top row. Reading the first six only ended 14.03's Rare Candy
//  step with "no rare-candy left", its RARE CANDY the 7th row (console B,
//  2026-11-14).
PocketRead read_whole_pocket(PocketList& list);

//  The bag's open pocket on the console, read whole (read_whole_pocket()),
//  its cursor on the top row to start; it ends where the read left it.
PocketRead read_open_pocket(ConsoleHandle& console, ProControllerContext& context, Language language);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
