/*  Menu Cursor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Moves the cursor of a fixed list or grid (a game's language select, the
 *  player's look) to an item by its index. The menu's shape is data; the
 *  presses follow from it. A D-pad probe tells whether a screen has a
 *  cursor at all, for menus no detector knows.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_MenuCursor_H
#define PokemonAutomation_NintendoSwitch_MenuCursor_H

#include <cstddef>
#include <optional>
#include <vector>
#include "CommonFramework/Tools/VideoStream.h"
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


//  How much the screen changed (pixel RMSD) around a D-pad probe, each against
//  the frame just before the press: over a wait with no input, after the press,
//  and after the opposite press.
struct CursorProbe{
    double still_change;
    double press_change;
    double back_change;
};
//  The least change a press that moved a cursor makes.
inline constexpr double CURSOR_PRESS_CHANGE = 6;
//  How many times what the screen changes on its own (an animated preview, a
//  blinking arrow) a press that moved a cursor changes it.
inline constexpr double CURSOR_NOISE_RATIO = 3;
//  Whether `probe` saw a cursor move and come back: the press changed the
//  screen by CURSOR_PRESS_CHANGE and CURSOR_NOISE_RATIO times the still
//  change, and the opposite press put back at least half of it.
bool cursor_probe_moved(const CursorProbe& probe);

//  The wait before a probe's first press, and after each press.
inline constexpr Milliseconds CURSOR_PROBE_WAIT = Milliseconds(400);
//  Waits with no input, presses `there`, waits, presses `back`, waits, and
//  measures the screen at each step. On a screen with no cursor `there` and
//  `back` do nothing; on one with a cursor they leave it where it was, unless
//  `there` meets an edge that does not wrap.
CursorProbe probe_cursor(VideoStream& stream, ProControllerContext& context, DpadPosition there, DpadPosition back);

//  Presses `moves` in order, one D-pad tap each.
void press_cursor_moves(ProControllerContext& context, const std::vector<DpadPosition>& moves);



} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
