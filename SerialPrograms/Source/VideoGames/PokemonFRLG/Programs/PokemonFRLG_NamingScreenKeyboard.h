/*  Naming Screen Keyboard
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The in-game keyboard used to name the player, the rival, PC boxes and Pokemon, and a
 *  planner that turns a name into button presses. Pure data and logic: no console needed.
 *
 *  Everything here follows pret/pokefirered src/naming_screen.c at commit
 *  ce6406d8c2eaf46d543c8a2606de0c3cb1951982 (English game). The decomp includes the Switch
 *  revision (REVISION >= 0xA), so the layout is the one on the Switch release.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_NamingScreenKeyboard_H
#define PokemonAutomation_PokemonFRLG_NamingScreenKeyboard_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


// The keyboard pages, in the order SELECT cycles through them. The screen opens on UPPER.
enum class NamingScreenPage : uint8_t{
    UPPER,
    LOWER,
    SYMBOLS,
};
constexpr size_t NAMING_SCREEN_PAGES = 3;
constexpr size_t NAMING_SCREEN_KEY_ROWS = 4;
constexpr size_t NAMING_SCREEN_MAX_KEY_COLUMNS = 8;

// Key columns on each page (sPageColumnCounts). The button column (page / back / OK) comes
// right after the last key column, so on a page with N key columns it is column N.
constexpr uint8_t NAMING_SCREEN_KEY_COLUMNS[NAMING_SCREEN_PAGES] = {8, 8, 6};

// The text each key types, in UTF-8 (sKeyboardChars), indexed [page][row][column].
// nullptr: no key (past the page's last column).
// The decomp pads each row with byte 0x00, which is a space in the game's charmap, and the
// keyboard labels draw a blank there: those cells type a space just like the spaces in rows 0-1.
// The apostrophe key types the game's right single quote (charmap 0xB4).
constexpr const char* NAMING_SCREEN_KEYS
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
        {"0", "1", "2", "3", "4", " ", nullptr, nullptr},
        {"5", "6", "7", "8", "9", " ", nullptr, nullptr},
        {"!", "?", "\xE2\x99\x82", "\xE2\x99\x80", "/", "-", nullptr, nullptr},     //  ! ? male female / -
        {"\xE2\x80\xA6", "\xE2\x80\x9C", "\xE2\x80\x9D", "\xE2\x80\x98", "'", " ", nullptr, nullptr},  //  ellipsis, double quotes, left single quote, '
    },
};

// The three buttons in the button column, top to bottom.
constexpr uint8_t NAMING_SCREEN_BUTTON_PAGE = 0;
constexpr uint8_t NAMING_SCREEN_BUTTON_BACK = 1;
constexpr uint8_t NAMING_SCREEN_BUTTON_OK = 2;


// Which naming screen is open; it sets how many characters fit (NamingScreenTemplate.maxChars).
enum class NamingScreenKind : uint8_t{
    PLAYER,     //  PLAYER_NAME_LENGTH
    RIVAL,      //  PLAYER_NAME_LENGTH
    BOX,        //  BOX_NAME_LENGTH
    POKEMON,    //  POKEMON_NAME_LENGTH, for caught Pokemon and the nickname rater
};
size_t naming_screen_max_chars(NamingScreenKind kind);
const char* naming_screen_kind_name(NamingScreenKind kind);


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
std::string to_string(const NamingCursor& cursor);


enum class NamingAction : uint8_t{
    UP,
    DOWN,
    LEFT,
    RIGHT,
    A,
    SELECT,     //  next page
    START,      //  move the cursor to OK
};
const char* naming_action_name(NamingAction action);
// Space-separated action names, e.g. "LEFT LEFT A START A".
std::string to_string(const std::vector<NamingAction>& actions);


// Where the cursor ends up after one press (HandleDpadMovement, MainState_WaitPageSwap,
// MoveCursorToOKButton). A leaves the cursor in place.
NamingCursor apply_naming_action(const NamingCursor& cursor, NamingAction action);

// Why the game can't take `text` as a name of this kind, or "" if it can: a character with
// no key, more characters than fit, or nothing but spaces (the game then keeps no name).
std::string naming_text_error(const std::string& text, NamingScreenKind kind);

// The presses that type `text` from `start` and confirm it: the shortest moves to each
// character's key, A, and at the end START and A to press OK.
// Throws InternalProgramError if naming_text_error() rejects `text`; check that first.
std::vector<NamingAction> plan_naming_input(
    const std::string& text, NamingScreenKind kind, const NamingCursor& start = NamingCursor()
);



}
}
}
#endif
