/*  Naming Screen Keys
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The in-game keyboard's layout, as pret/pokefirered src/naming_screen.c at commit
 *  ce6406d8c2eaf46d543c8a2606de0c3cb1951982 (English game) has it. The decomp includes the
 *  Switch revision (REVISION >= 0xA), so the layout is the one on the Switch release.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_NamingScreenKeys_H
#define PokemonAutomation_PokemonFRLG_NamingScreenKeys_H

#include <stddef.h>
#include <stdint.h>
#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


// The keyboard pages, in the order SELECT cycles through them. The screen opens on UPPER.
enum class NamingScreenPage : uint8_t{
    UPPER,
    LOWER,
    SYMBOLS,
};
//  Total page count on naming screen.
constexpr size_t NAMING_SCREEN_PAGES = 3;
//  Number of key rows per page.
constexpr size_t NAMING_SCREEN_KEY_ROWS = 4;
//  Maximum key columns across pages.
constexpr size_t NAMING_SCREEN_MAX_KEY_COLUMNS = 8;

// Key columns on each page (sPageColumnCounts). The button column (page / back / OK) comes
// right after the last key column, so on a page with N key columns it is column N.
constexpr uint8_t NAMING_SCREEN_KEY_COLUMNS[NAMING_SCREEN_PAGES] = {8, 8, 6};

// The text each key types, in UTF-8 (sKeyboardChars), indexed [page][row][column].
// Empty: no key (past the page's last column).
// The decomp pads each row with byte 0x00, which is a space in the game's charmap, and the
// keyboard labels draw a blank there: those cells type a space just like the spaces in rows 0-1.
// The apostrophe key types the game's right single quote (charmap 0xB4).
constexpr std::string_view NAMING_SCREEN_KEYS
    [NAMING_SCREEN_PAGES][NAMING_SCREEN_KEY_ROWS][NAMING_SCREEN_MAX_KEY_COLUMNS] = {
    {   //  UPPER
        {"A", "B", "C", "D", "E", "F", " ", "."},
        {"G", "H", "I", "J", "K", "L", " ", ","},
        {"M", "N", "O", "P", "Q", "R", "S", " "},
        {"T", "U", "V", "W", "X", "Y", "Z", " "},
    },
    {   //  LOWER
        {"a", "b", "c", "d", "e", "f", " ", "."},
        {"g", "h", "i", "j", "k", "l", " ", ","},
        {"m", "n", "o", "p", "q", "r", "s", " "},
        {"t", "u", "v", "w", "x", "y", "z", " "},
    },
    {   //  SYMBOLS
        {"0", "1", "2", "3", "4", " ", "", ""},
        {"5", "6", "7", "8", "9", " ", "", ""},
        {"!", "?", "\xE2\x99\x82", "\xE2\x99\x80", "/", "-", "", ""},     //  ! ? male female / -
        {"\xE2\x80\xA6", "\xE2\x80\x9C", "\xE2\x80\x9D", "\xE2\x80\x98", "'", " ", "", ""},  //  ellipsis, double quotes, left single quote, '
    },
};

// The three buttons in the button column, top to bottom.
//  Button index for switching character pages.
constexpr uint8_t NAMING_SCREEN_BUTTON_PAGE = 0;
//  Button index for backspacing a character.
constexpr uint8_t NAMING_SCREEN_BUTTON_BACK = 1;
//  Button index for confirming naming input.
constexpr uint8_t NAMING_SCREEN_BUTTON_OK = 2;



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
