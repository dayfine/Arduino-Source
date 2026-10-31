/*  Naming Screen Keyboard
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string.h>
#include "Common/Compiler.h"
#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_NamingScreenKeyboard.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


size_t naming_screen_max_chars(NamingScreenKind kind){
    switch (kind){
    case NamingScreenKind::PLAYER:  return 7;
    case NamingScreenKind::RIVAL:   return 7;
    case NamingScreenKind::BOX:     return 8;
    case NamingScreenKind::POKEMON: return 10;
    }
    return 0;
}
const char* naming_screen_kind_name(NamingScreenKind kind){
    switch (kind){
    case NamingScreenKind::PLAYER:  return "player";
    case NamingScreenKind::RIVAL:   return "rival";
    case NamingScreenKind::BOX:     return "box";
    case NamingScreenKind::POKEMON: return "Pokemon";
    }
    return "?";
}

std::string to_string(const NamingCursor& cursor){
    static const char* const PAGE_NAMES[] = {"UPPER", "LOWER", "SYMBOLS"};
    return std::string(PAGE_NAMES[(size_t)cursor.page])
        + " row " + std::to_string(cursor.row)
        + " column " + std::to_string(cursor.column)
        + " (remembered row " + std::to_string(cursor.remembered_row) + ")";
}

const char* naming_action_name(NamingAction action){
    switch (action){
    case NamingAction::UP:      return "UP";
    case NamingAction::DOWN:    return "DOWN";
    case NamingAction::LEFT:    return "LEFT";
    case NamingAction::RIGHT:   return "RIGHT";
    case NamingAction::A:       return "A";
    case NamingAction::SELECT:  return "SELECT";
    case NamingAction::START:   return "START";
    }
    return "?";
}
std::string to_string(const std::vector<NamingAction>& actions){
    std::string ret;
    for (NamingAction action : actions){
        if (!ret.empty()){
            ret += ' ';
        }
        ret += naming_action_name(action);
    }
    return ret;
}


namespace{

constexpr uint8_t BUTTON_COUNT = 3;
// sKeyRowToButtonRow and sButtonRowToKeyRow in HandleDpadMovement().
constexpr uint8_t KEY_ROW_TO_BUTTON[NAMING_SCREEN_KEY_ROWS] = {0, 1, 1, 2};
constexpr uint8_t BUTTON_TO_KEY_ROW[BUTTON_COUNT] = {0, 0, 3};

uint8_t key_columns(NamingScreenPage page){
    return NAMING_SCREEN_KEY_COLUMNS[(size_t)page];
}

//  Wraps the row after a move: three buttons on the button column, four key rows elsewhere.
void wrap_row(NamingCursor& cursor, bool on_button_column, int y){
    if (on_button_column){
        if (y < 0){
            y = BUTTON_COUNT - 1;
        }
        if (y >= BUTTON_COUNT){
            y = 0;
        }
        //  The game reuses tButtonId here, so the middle button can return to row 1 or 2.
        if (y == 0){
            cursor.remembered_row = NAMING_SCREEN_BUTTON_BACK;
        }else if (y == BUTTON_COUNT - 1){
            cursor.remembered_row = NAMING_SCREEN_BUTTON_OK;
        }
    }else{
        if (y < 0){
            y = NAMING_SCREEN_KEY_ROWS - 1;
        }
        if (y >= (int)NAMING_SCREEN_KEY_ROWS){
            y = 0;
        }
    }
    cursor.row = (uint8_t)y;
}

NamingCursor move(NamingCursor cursor, int dx, int dy){
    const int columns = key_columns(cursor.page);
    const int previous_x = cursor.column;
    int x = cursor.column + dx;
    int y = cursor.row + dy;

    //  Wrap X. The button column is part of the ring.
    if (x < 0){
        x = columns;
    }
    if (x > columns){
        x = 0;
    }

    //  Moving onto or off the button column.
    if (dx != 0){
        if (x == columns){
            cursor.remembered_row = (uint8_t)y;
            y = KEY_ROW_TO_BUTTON[y];
        }else if (previous_x == columns){
            y = y == BUTTON_COUNT / 2 ? cursor.remembered_row : BUTTON_TO_KEY_ROW[y];
        }
    }

    cursor.column = (uint8_t)x;
    wrap_row(cursor, x == columns, y);
    return cursor;
}

NamingCursor next_page(NamingCursor cursor){
    const bool on_button_column = cursor.column == key_columns(cursor.page);
    cursor.page = (NamingScreenPage)(((size_t)cursor.page + 1) % NAMING_SCREEN_PAGES);
    const uint8_t columns = key_columns(cursor.page);
    if (on_button_column || cursor.column >= columns){
        cursor.column = on_button_column ? columns : columns - 1;
    }
    return cursor;
}

const char* key_at(const NamingCursor& cursor){
    if (cursor.column >= key_columns(cursor.page)){
        return nullptr;
    }
    return NAMING_SCREEN_KEYS[(size_t)cursor.page][cursor.row][cursor.column];
}


struct KeyAlias{
    const char* typed;
    const char* key;
};
// Other spellings of a key's character.
constexpr KeyAlias KEY_ALIASES[] = {
    {"\xE2\x80\x99", "'"},     //  right single quote: the apostrophe key types it
};

bool text_has_at(const std::string& text, size_t position, const char* str){
    return text.compare(position, strlen(str), str) == 0;
}

// The key text at `position` in `text`, or nullptr if no key types it.
const char* key_text_at(const std::string& text, size_t position, size_t& length){
    const char* best = nullptr;
    length = 0;
    for (const auto& page : NAMING_SCREEN_KEYS){
        for (const auto& row : page){
            for (const char* key : row){
                if (key != nullptr && strlen(key) > length && text_has_at(text, position, key)){
                    best = key;
                    length = strlen(key);
                }
            }
        }
    }
    for (const KeyAlias& alias : KEY_ALIASES){
        if (strlen(alias.typed) > length && text_has_at(text, position, alias.typed)){
            best = alias.key;
            length = strlen(alias.typed);
        }
    }
    return best;
}

// The UTF-8 character at `position`, for error messages.
std::string utf8_char_at(const std::string& text, size_t position){
    const unsigned char lead = (unsigned char)text[position];
    size_t length = 1;
    if (lead >= 0xF0){
        length = 4;
    }else if (lead >= 0xE0){
        length = 3;
    }else if (lead >= 0xC0){
        length = 2;
    }
    return text.substr(position, length);
}

// Splits `text` into the keys that type it. Returns the error, or "" on success.
std::string split_into_keys(const std::string& text, std::vector<const char*>& keys){
    keys.clear();
    for (size_t position = 0; position < text.size();){
        size_t length;
        const char* key = key_text_at(text, position, length);
        if (key == nullptr){
            return "\"" + text + "\" has a character the naming screen can't type: \""
                + utf8_char_at(text, position) + "\" (byte " + std::to_string(position) + "). "
                "It types A-Z, a-z, 0-9, space and . , ! ? / - ' and the game's "
                "male/female signs, ellipsis and curly quotes.";
        }
        keys.emplace_back(key);
        position += length;
    }
    return "";
}


size_t state_index(const NamingCursor& cursor){
    return (((size_t)cursor.page * (NAMING_SCREEN_MAX_KEY_COLUMNS + 1) + cursor.column)
        * NAMING_SCREEN_KEY_ROWS + cursor.row) * NAMING_SCREEN_KEY_ROWS + cursor.remembered_row;
}
constexpr size_t STATE_COUNT =
    NAMING_SCREEN_PAGES * (NAMING_SCREEN_MAX_KEY_COLUMNS + 1) * NAMING_SCREEN_KEY_ROWS * NAMING_SCREEN_KEY_ROWS;

// Breadth-first search over cursor states for the fewest presses that reach a key typing `key`.
// Appends the moves and returns the cursor on that key.
NamingCursor append_moves_to_key(const NamingCursor& start, const char* key, std::vector<NamingAction>& actions){
    //  START is left out: it only jumps to OK, which the plan uses once at the end.
    static const NamingAction MOVES[] = {
        NamingAction::UP, NamingAction::DOWN, NamingAction::LEFT, NamingAction::RIGHT, NamingAction::SELECT,
    };
    struct Visit{
        bool seen = false;
        size_t parent = 0;
        NamingAction action = NamingAction::A;
    };
    std::vector<Visit> visits(STATE_COUNT);
    std::vector<NamingCursor> queue{start};
    visits[state_index(start)].seen = true;

    for (size_t head = 0; head < queue.size(); head++){
        const NamingCursor cursor = queue[head];
        const char* here = key_at(cursor);
        if (here != nullptr && strcmp(here, key) == 0){
            std::vector<NamingAction> path;
            for (size_t index = state_index(cursor); index != state_index(start); index = visits[index].parent){
                path.emplace_back(visits[index].action);
            }
            actions.insert(actions.end(), path.rbegin(), path.rend());
            return cursor;
        }
        for (NamingAction action : MOVES){
            const NamingCursor next = apply_naming_action(cursor, action);
            Visit& visit = visits[state_index(next)];
            if (!visit.seen){
                visit.seen = true;
                visit.parent = state_index(cursor);
                visit.action = action;
                queue.emplace_back(next);
            }
        }
    }
    //  Every key is reachable, so this is a bug.
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string("No path to naming screen key: ") + key);
}

}


NamingCursor apply_naming_action(const NamingCursor& cursor, NamingAction action){
    switch (action){
    case NamingAction::UP:      return move(cursor, 0, -1);
    case NamingAction::DOWN:    return move(cursor, 0, 1);
    case NamingAction::LEFT:    return move(cursor, -1, 0);
    case NamingAction::RIGHT:   return move(cursor, 1, 0);
    case NamingAction::SELECT:  return next_page(cursor);
    case NamingAction::START:{
        NamingCursor ret = cursor;
        ret.column = key_columns(cursor.page);
        ret.row = NAMING_SCREEN_BUTTON_OK;
        return ret;
    }
    case NamingAction::A:
        return cursor;
    }
    return cursor;
}


std::string naming_text_error(const std::string& text, NamingScreenKind kind){
    std::vector<const char*> keys;
    std::string error = split_into_keys(text, keys);
    if (!error.empty()){
        return error;
    }
    bool only_spaces = true;
    for (const char* key : keys){
        only_spaces &= strcmp(key, " ") == 0;
    }
    if (only_spaces){
        return "The name \"" + text + "\" is empty or only spaces, so the game would not keep it.";
    }
    const size_t max_chars = naming_screen_max_chars(kind);
    if (keys.size() > max_chars){
        return "\"" + text + "\" is " + std::to_string(keys.size()) + " characters, but a "
            + naming_screen_kind_name(kind) + " name holds at most " + std::to_string(max_chars) + ".";
    }
    return "";
}

std::vector<NamingAction> plan_naming_input(
    const std::string& text, NamingScreenKind kind, const NamingCursor& start
){
    const std::string error = naming_text_error(text, kind);
    if (!error.empty()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, error);
    }
    std::vector<const char*> keys;
    split_into_keys(text, keys);

    std::vector<NamingAction> actions;
    NamingCursor cursor = start;
    for (const char* key : keys){
        cursor = append_moves_to_key(cursor, key, actions);
        actions.emplace_back(NamingAction::A);
    }
    //  When the last key fills the name, the game moves the cursor to OK by itself; START is
    //  then harmless.
    actions.emplace_back(NamingAction::START);
    actions.emplace_back(NamingAction::A);
    return actions;
}



}
}
}
