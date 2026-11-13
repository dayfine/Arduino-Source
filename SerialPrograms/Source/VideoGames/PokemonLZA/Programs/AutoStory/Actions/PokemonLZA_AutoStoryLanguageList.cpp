/*  PokemonLZA AutoStory - New game language list
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_AutoStoryLanguageList.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::optional<size_t> language_list_index(Language language){
    switch (language){
    case Language::English:             return 0;
    case Language::Spanish:             return 1;
    case Language::French:              return 3;
    case Language::German:              return 4;
    case Language::Italian:             return 5;
    case Language::Japanese:            return 6;
    case Language::Korean:              return 7;
    case Language::ChineseSimplified:   return 8;
    case Language::ChineseTraditional:  return 9;
    default:                            return std::nullopt;
    }
}

std::optional<std::vector<IntroKey>> language_list_moves(size_t from, size_t to){
    if (from >= LANGUAGE_LIST_SIZE || to >= LANGUAGE_LIST_SIZE){
        return std::nullopt;
    }
    const size_t from_column = from / LANGUAGE_LIST_ROWS;
    const size_t from_row = from % LANGUAGE_LIST_ROWS;
    const size_t to_column = to / LANGUAGE_LIST_ROWS;
    const size_t to_row = to % LANGUAGE_LIST_ROWS;
    std::vector<IntroKey> moves;
    if (to_column > from_column){
        moves.insert(moves.end(), to_column - from_column, IntroKey::RIGHT);
    }else{
        moves.insert(moves.end(), from_column - to_column, IntroKey::LEFT);
    }
    if (to_row > from_row){
        moves.insert(moves.end(), to_row - from_row, IntroKey::DOWN);
    }else{
        moves.insert(moves.end(), from_row - to_row, IntroKey::UP);
    }
    return moves;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
