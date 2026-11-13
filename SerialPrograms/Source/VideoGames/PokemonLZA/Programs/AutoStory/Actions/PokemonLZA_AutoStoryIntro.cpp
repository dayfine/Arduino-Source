/*  PokemonLZA AutoStory - New game intro
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <format>
#include "PokemonLZA_AutoStoryIntro.h"
#include "PokemonLZA_AutoStoryLanguageList.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr size_t LOOK_COLUMNS = 4;

} // namespace


std::optional<std::vector<IntroKey>> look_picker_moves(size_t from, size_t to){
    if (from < 1 || from > LOOK_COUNT || to < 1 || to > LOOK_COUNT){
        return std::nullopt;
    }
    const size_t from_row = (from - 1) / LOOK_COLUMNS;
    const size_t from_column = (from - 1) % LOOK_COLUMNS;
    const size_t to_row = (to - 1) / LOOK_COLUMNS;
    const size_t to_column = (to - 1) % LOOK_COLUMNS;
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


std::optional<NameKeys> name_keys(std::string_view name, bool first_letter_auto_caps){
    if (name.empty()){
        return std::nullopt;
    }
    NameKeys result;
    for (size_t i = 0; i < name.size(); i++){
        const unsigned char ch = (unsigned char)name[i];
        if (ch > 127 || !std::isalnum(ch)){
            return std::nullopt;
        }
        if (i > 0 && std::isupper(ch)){
            return std::nullopt;
        }
        result.keys.push_back((char)std::toupper(ch));
    }
    const unsigned char first = (unsigned char)name[0];
    if (std::isalpha(first)){
        result.shift_first = (std::isupper(first) != 0) != first_letter_auto_caps;
    }
    return result;
}


std::string new_game_problem(Language language, const NewGameChoices& choices){
    if (!language_list_index(language)){
        return "The new game's language list has no such language.";
    }
    if (choices.look < 1 || choices.look > LOOK_COUNT){
        return std::format("The player look must be 1 to {}.", LOOK_COUNT);
    }
    if (!name_keys(choices.name, choices.name_first_letter_auto_caps)){
        return std::format(
            "The player name \"{}\" must be letters and digits, with a capital only as its first letter.", choices.name
        );
    }
    return std::string();
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
