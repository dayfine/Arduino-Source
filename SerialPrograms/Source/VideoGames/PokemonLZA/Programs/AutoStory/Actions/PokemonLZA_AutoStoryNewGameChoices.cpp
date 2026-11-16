/*  PokemonLZA AutoStory - New game choices
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <format>
#include "PokemonLZA_AutoStoryNameKeys.h"
#include "PokemonLZA_AutoStoryNewGameChoices.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The language list's rows per column.
constexpr size_t LANGUAGE_LIST_ROWS = 5;
constexpr size_t LANGUAGE_LIST_SIZE = 2 * LANGUAGE_LIST_ROWS;

//  The look picker: 2 rows of LOOK_COLUMNS portraits.
constexpr size_t LOOK_COLUMNS = 4;
constexpr size_t LOOK_COUNT = 2 * LOOK_COLUMNS;

//  The presses from (column, row) `from` to `to` on a grid: across first,
//  then up or down.
std::vector<IntroKey> grid_moves(size_t from_column, size_t from_row, size_t to_column, size_t to_row){
    std::vector<IntroKey> moves;
    for (size_t c = from_column; c < to_column; c++){
        moves.push_back(IntroKey::RIGHT);
    }
    for (size_t c = to_column; c < from_column; c++){
        moves.push_back(IntroKey::LEFT);
    }
    for (size_t r = from_row; r < to_row; r++){
        moves.push_back(IntroKey::DOWN);
    }
    for (size_t r = to_row; r < from_row; r++){
        moves.push_back(IntroKey::UP);
    }
    return moves;
}

} // namespace


const NewGameLanguageInfo& new_game_language_info(NewGameLanguage language){
    static const std::array<NewGameLanguageInfo, LANGUAGE_LIST_SIZE> table{{
        {.tag = "en",      .list_entry = 0, .name_limit = 12, .ocr_language = Language::English,            .confirm_text = "English"},
        {.tag = "es-ES",   .list_entry = 1, .name_limit = 12, .ocr_language = Language::Spanish,            .confirm_text = "Espa\xc3\xb1" "a)"},
        {.tag = "es-419",  .list_entry = 2, .name_limit = 12, .ocr_language = Language::Spanish,            .confirm_text = "Latino"},
        {.tag = "fr",      .list_entry = 3, .name_limit = 12, .ocr_language = Language::French,             .confirm_text = "Fran"},
        {.tag = "de",      .list_entry = 4, .name_limit = 12, .ocr_language = Language::German,             .confirm_text = "Deutsch"},
        {.tag = "it",      .list_entry = 5, .name_limit = 12, .ocr_language = Language::Italian,            .confirm_text = "Italiano"},
        {.tag = "ja",      .list_entry = 6, .name_limit = 6,  .ocr_language = Language::Japanese,           .confirm_text = "\xe6\x97\xa5\xe6\x9c\xac"},
        {.tag = "ko",      .list_entry = 7, .name_limit = 6,  .ocr_language = Language::Korean,             .confirm_text = "\xed\x95\x9c\xea\xb5\xad", .supported = false},
        {.tag = "zh-Hans", .list_entry = 8, .name_limit = 6,  .ocr_language = Language::ChineseSimplified,  .confirm_text = "\xe7\xae\x80\xe4\xbd\x93"},
        {.tag = "zh-Hant", .list_entry = 9, .name_limit = 6,  .ocr_language = Language::ChineseTraditional, .confirm_text = "\xe7\xb9\x81\xe9\xab\x94"},
    }};
    const size_t index = (size_t)language;
    return table[index < table.size() ? index : 0];
}

std::optional<std::string> new_game_problem(const NewGameChoices& choices){
    const NewGameLanguageInfo& info = new_game_language_info(choices.language);
    if (!info.supported){
        return std::format("AutoStory: Korean ({}) is not supported yet: its name keyboard is Hangul.", info.tag);
    }
    if (choices.look < 1 || choices.look > LOOK_COUNT){
        return std::format("AutoStory: the player look must be 1 to {}, not {}.", LOOK_COUNT, choices.look);
    }
    if (choices.name.empty()){
        return std::string("AutoStory: the player name is empty.");
    }
    if (choices.name.size() > info.name_limit){
        return std::format(
            "AutoStory: the player name \"{}\" has {} characters; the game takes at most {} in this language ({}).",
            choices.name, choices.name.size(), info.name_limit, info.tag
        );
    }
    if (!name_key_presses(choices.name, choices.language)){
        return std::format(
            "AutoStory: the player name \"{}\" has a character that is not on the name keyboard's first page "
            "(letters, digits and - / : ' , . ? !).",
            choices.name
        );
    }
    return std::nullopt;
}

std::optional<std::vector<IntroKey>> language_list_moves(size_t from, size_t to){
    if (from >= LANGUAGE_LIST_SIZE || to >= LANGUAGE_LIST_SIZE){
        return std::nullopt;
    }
    return grid_moves(
        from / LANGUAGE_LIST_ROWS, from % LANGUAGE_LIST_ROWS,
        to / LANGUAGE_LIST_ROWS, to % LANGUAGE_LIST_ROWS
    );
}

std::optional<std::vector<IntroKey>> look_picker_moves(size_t from, size_t to){
    if (from < 1 || from > LOOK_COUNT || to < 1 || to > LOOK_COUNT){
        return std::nullopt;
    }
    return grid_moves(
        (from - 1) % LOOK_COLUMNS, (from - 1) / LOOK_COLUMNS,
        (to - 1) % LOOK_COLUMNS, (to - 1) / LOOK_COLUMNS
    );
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
