/*  Naming Screen Cursor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The naming screen's cursor and the presses that move it, as
 *  pret/pokefirered src/naming_screen.c moves them (see PokemonFRLG_NamingScreenKeys.h).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_NamingScreenCursor_H
#define PokemonAutomation_PokemonFRLG_NamingScreenCursor_H

#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>
#include "PokemonFRLG_NamingScreenKeys.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


// The cursor, as the game tracks it.
struct NamingCursor{
    NamingScreenPage page = NamingScreenPage::UPPER;
    // Key row (0-3), or button (0-2) when on the button column.
    uint8_t row = 0;
    // Key column, or NAMING_SCREEN_KEY_COLUMNS[page] for the button column.
    uint8_t column = 0;
    // The game's tButtonId: the key row to return to when leaving the middle button.
    uint8_t remembered_row = 0;

    bool operator==(const NamingCursor& other) const = default;
};
//  String representation of cursor coordinates.
std::string to_string(const NamingCursor& cursor);


//  Input action on the naming screen keyboard.
enum class NamingAction : uint8_t{
    UP,
    DOWN,
    LEFT,
    RIGHT,
    A,
    SELECT,     //  next page
    START,      //  move the cursor to OK
};
//  String representation of naming action.
std::string_view naming_action_name(NamingAction action);
// Space-separated action names, e.g. "LEFT LEFT A START A".
std::string to_string(const std::vector<NamingAction>& actions);


// Where the cursor ends up after one press (HandleDpadMovement, MainState_WaitPageSwap,
// MoveCursorToOKButton). A leaves the cursor in place.
NamingCursor apply_naming_action(const NamingCursor& cursor, NamingAction action);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
