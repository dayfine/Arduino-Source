/*  Menu Cursor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch_MenuCursor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

using namespace std::chrono_literals;


namespace{

void add_presses(std::vector<DpadPosition>& moves, DpadPosition direction, size_t count){
    moves.insert(moves.end(), count, direction);
}
void add_sideways(std::vector<DpadPosition>& moves, size_t from_column, size_t to_column){
    if (to_column > from_column){
        add_presses(moves, DPAD_RIGHT, to_column - from_column);
    }else{
        add_presses(moves, DPAD_LEFT, from_column - to_column);
    }
}

} // namespace


std::optional<std::vector<DpadPosition>> menu_cursor_moves(const MenuGrid& grid, size_t from, size_t to){
    if (grid.columns == 0 || from >= grid.count || to >= grid.count){
        return std::nullopt;
    }
    const size_t from_row = from / grid.columns;
    const size_t from_column = from % grid.columns;
    const size_t to_row = to / grid.columns;
    const size_t to_column = to % grid.columns;
    std::vector<DpadPosition> moves;
    if (to_row > from_row){
        //  The start's row is above the target's, so it is full: sideways first.
        add_sideways(moves, from_column, to_column);
        add_presses(moves, DPAD_DOWN, to_row - from_row);
    }else{
        //  Up to the target's row first, which is full unless it is the start's.
        add_presses(moves, DPAD_UP, from_row - to_row);
        add_sideways(moves, from_column, to_column);
    }
    return moves;
}


void press_cursor_moves(ProControllerContext& context, const std::vector<DpadPosition>& moves){
    for (DpadPosition move : moves){
        pbf_press_dpad(context, move, 80ms, 320ms);
    }
}



} // namespace NintendoSwitch
} // namespace PokemonAutomation
