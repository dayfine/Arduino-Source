/*  Menu Cursor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Moves the cursor of a fixed list or grid (a game's language select, the
 *  player's look) to an item by its index. The menu's shape is data; the
 *  presses follow from it; a detector for the menu reads where the cursor is.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_MenuCursor_H
#define PokemonAutomation_NintendoSwitch_MenuCursor_H

#include <cstddef>
#include <optional>
#include <vector>
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  A menu of `count` items in rows of `columns`, numbered left to right, then
//  top to bottom; a list has one column. Only the last row may be short.
struct MenuGrid{
    size_t columns = 1;
    size_t count = 0;
};

//  The D-pad presses that take the cursor from item `from` to item `to`.
//  None crosses an edge, so they hold whether or not the menu wraps, and
//  sideways presses happen on a full row. nullopt when either item is outside
//  the grid.
std::optional<std::vector<DpadPosition>> menu_cursor_moves(const MenuGrid& grid, size_t from, size_t to);

//  Presses `moves` in order, one D-pad tap each.
void press_cursor_moves(ProControllerContext& context, const std::vector<DpadPosition>& moves);



} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
