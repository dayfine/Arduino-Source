/*  PokemonLZA AutoStory - Name keys
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <iterator>
#include "PokemonLZA_AutoStoryNameKeys.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The first page's character rows, as the header draws them.
constexpr std::string_view KEY_ROWS[] = {
    "1234567890-",
    "qwertyuiop/",
    "asdfghjkl:'",
    "zxcvbnm,.?!",
};
constexpr size_t BOTTOM_ROW = 4;

//  Simplified Chinese's "English" mode key: the bottom row's fourth key,
//  under "4".
constexpr size_t ENGLISH_MODE_COLUMN = 3;

struct KeyPosition{
    size_t row = 0;
    size_t col = 0;
};

std::optional<KeyPosition> key_position(char lower){
    for (size_t row = 0; row < std::size(KEY_ROWS); row++){
        const size_t col = KEY_ROWS[row].find(lower);
        if (col != std::string_view::npos){
            return KeyPosition{.row = row, .col = col};
        }
    }
    return std::nullopt;
}

//  Across, then up or down; but up first off the bottom row, whose keys
//  are wider than a column: "English" is the fourth key along it, and the
//  key above it is "4" (the frames).
void move_to(std::vector<IntroKey>& presses, KeyPosition& cursor, KeyPosition target){
    for (; cursor.row == BOTTOM_ROW && cursor.row > target.row; cursor.row--){
        presses.push_back(IntroKey::UP);
    }
    for (; cursor.col < target.col; cursor.col++){
        presses.push_back(IntroKey::RIGHT);
    }
    for (; cursor.col > target.col; cursor.col--){
        presses.push_back(IntroKey::LEFT);
    }
    for (; cursor.row < target.row; cursor.row++){
        presses.push_back(IntroKey::DOWN);
    }
    for (; cursor.row > target.row; cursor.row--){
        presses.push_back(IntroKey::UP);
    }
}

} // namespace


std::optional<std::vector<IntroKey>> name_key_presses(std::string_view name, NewGameLanguage language){
    if (name.empty()){
        return std::nullopt;
    }
    std::vector<IntroKey> presses;
    KeyPosition cursor;
    if (language == NewGameLanguage::ChineseSimplified){
        move_to(presses, cursor, KeyPosition{.row = BOTTOM_ROW, .col = 0});
        move_to(presses, cursor, KeyPosition{.row = BOTTOM_ROW, .col = ENGLISH_MODE_COLUMN});
        presses.push_back(IntroKey::A);
    }
    for (const char ch : name){
        const unsigned char byte = (unsigned char)ch;
        const std::optional<KeyPosition> target = key_position((char)std::tolower(byte));
        if (byte >= 0x80 || !target){
            return std::nullopt;
        }
        move_to(presses, cursor, *target);
        if (std::isupper(byte)){
            presses.push_back(IntroKey::SHIFT);
        }
        presses.push_back(IntroKey::A);
    }
    presses.push_back(IntroKey::PLUS);
    return presses;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
