/*  Menu Cursor - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "NintendoSwitch_MenuCursor.h"
#include "NintendoSwitch_MenuCursor_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

namespace{


std::string describe(const std::vector<DpadPosition>& moves){
    std::string out;
    for (DpadPosition move : moves){
        out += (out.empty() ? "" : " ") + dpad_to_string(move);
    }
    return out.empty() ? "(none)" : out;
}

struct MovesInput{
    MenuGrid grid;
    size_t from;
    size_t to;
};

struct MovesCase{
    std::string_view name;
    MovesInput input;
    std::vector<DpadPosition> expectation;
};

UnitTestResult test_cursor_moves(){
    const MenuGrid list{1, 9};
    const MenuGrid looks{4, 8};
    //  Seven items in rows of three: the last row holds one.
    const MenuGrid ragged{3, 7};
    const MovesCase cases[] = {
        {"list: stay",                  {list,   1, 1}, {}},
        {"list: down",                  {list,   1, 2}, {DPAD_DOWN}},
        {"list: up to the top",         {list,   1, 0}, {DPAD_UP}},
        {"list: top to bottom, no wrap", {list,  0, 8}, std::vector<DpadPosition>(8, DPAD_DOWN)},
        {"grid: right on the top row",  {looks,  0, 2}, {DPAD_RIGHT, DPAD_RIGHT}},
        {"grid: down a row",            {looks,  0, 4}, {DPAD_DOWN}},
        {"grid: sideways, then down",   {looks,  1, 7}, {DPAD_RIGHT, DPAD_RIGHT, DPAD_DOWN}},
        {"grid: up, then sideways",     {looks,  7, 0}, {DPAD_UP, DPAD_LEFT, DPAD_LEFT, DPAD_LEFT}},
        //  The short row has only column 0: sideways happens on the full row above.
        {"ragged: into the short row",  {ragged, 2, 6}, {DPAD_LEFT, DPAD_LEFT, DPAD_DOWN, DPAD_DOWN}},
        {"ragged: out of the short row", {ragged, 6, 5}, {DPAD_UP, DPAD_RIGHT, DPAD_RIGHT}},
    };
    for (const MovesCase& c : cases){
        const std::optional<std::vector<DpadPosition>> moves = menu_cursor_moves(c.input.grid, c.input.from, c.input.to);
        if (!moves){
            return std::string(c.name) + ": no moves.";
        }
        if (*moves != c.expectation){
            return std::string(c.name) + ": " + describe(*moves) + ", expected " + describe(c.expectation) + ".";
        }
    }
    if (menu_cursor_moves(looks, 0, 8) || menu_cursor_moves(looks, 8, 0) || menu_cursor_moves(MenuGrid{0, 4}, 0, 1)){
        return "An item outside the grid, or a grid with no columns, must give no moves.";
    }
    return true;
}

//  Walks the moves from `from` to `to` on `grid`; what went wrong, or "".
std::string check_walk(const MenuGrid& grid, size_t from, size_t to){
    const std::string pair = std::to_string(grid.columns) + "x" + std::to_string(grid.count) +
        " from " + std::to_string(from) + " to " + std::to_string(to);
    const std::optional<std::vector<DpadPosition>> moves = menu_cursor_moves(grid, from, to);
    if (!moves){
        return pair + ": no moves.";
    }
    long row = (long)(from / grid.columns);
    long column = (long)(from % grid.columns);
    for (DpadPosition move : *moves){
        row += move == DPAD_DOWN ? 1 : move == DPAD_UP ? -1 : 0;
        column += move == DPAD_RIGHT ? 1 : move == DPAD_LEFT ? -1 : 0;
        const long index = row * (long)grid.columns + column;
        if (row < 0 || column < 0 || column >= (long)grid.columns || index >= (long)grid.count){
            return pair + ": " + describe(*moves) + " leaves the grid.";
        }
    }
    if (row * (long)grid.columns + column != (long)to){
        return pair + ": " + describe(*moves) + " ends elsewhere.";
    }
    return "";
}

//  Every pair of items in a few shapes: the moves land on the target and never
//  leave the grid or step on a missing item.
UnitTestResult test_cursor_moves_stay_inside(){
    for (const MenuGrid& grid : {MenuGrid{1, 9}, MenuGrid{4, 8}, MenuGrid{3, 7}, MenuGrid{5, 1}}){
        for (size_t from = 0; from < grid.count; from++){
            for (size_t to = 0; to < grid.count; to++){
                const std::string error = check_walk(grid, from, to);
                if (!error.empty()){
                    return error;
                }
            }
        }
    }
    return true;
}

struct MenuCursorInput{
    UnitTestResult (*body)();
};

struct MenuCursorExpectation{
};

struct MenuCursorCase{
    std::string name;
    MenuCursorInput input;
    MenuCursorExpectation expectation;
};

class Test_MenuCursor : public UnitTest{
public:
    explicit Test_MenuCursor(MenuCursorCase test_case)
        : UnitTest("NintendoSwitch::MenuCursor - " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.body();
    }

private:
    MenuCursorCase m_case;
};

const MenuCursorCase MENU_CURSOR_CASES[] = {
    {
        .name = "presses from item to item",
        .input = {test_cursor_moves},
        .expectation = {},
    },
    {
        .name = "presses stay inside the grid",
        .input = {test_cursor_moves_stay_inside},
        .expectation = {},
    },
};


} // namespace


void add_tests_MenuCursor(UnitTestDatabase& database){
    for (const MenuCursorCase& item : MENU_CURSOR_CASES){
        database.add<Test_MenuCursor>(item);
    }
}



} // namespace NintendoSwitch
} // namespace PokemonAutomation
