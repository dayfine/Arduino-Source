/*  PokemonLZA AutoStory - New game intro
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <format>
#include "PokemonLZA_AutoStoryIntro.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr size_t LOOK_COLUMNS = 4;

std::optional<size_t> language_list_index(Language language){
    switch (language){
    case Language::English:             return 0;
    case Language::Spanish:             return 1;
    case Language::French:              return 2;
    case Language::German:              return 3;
    case Language::Italian:             return 4;
    case Language::Japanese:            return 5;
    case Language::Korean:              return 6;
    case Language::ChineseSimplified:   return 7;
    case Language::ChineseTraditional:  return 8;
    default:                            return std::nullopt;
    }
}

} // namespace


std::optional<std::vector<IntroKey>> language_list_presses(Language language){
    const std::optional<size_t> index = language_list_index(language);
    if (!index){
        return std::nullopt;
    }
    std::vector<IntroKey> presses(*index, IntroKey::DOWN);
    presses.push_back(IntroKey::A);
    return presses;
}


std::optional<std::vector<IntroKey>> look_picker_presses(size_t look){
    if (look < 1 || look > LOOK_COUNT){
        return std::nullopt;
    }
    const size_t row = (look - 1) / LOOK_COLUMNS;
    const size_t column = (look - 1) % LOOK_COLUMNS;
    std::vector<IntroKey> presses;
    presses.push_back(IntroKey::UP);
    presses.insert(presses.end(), LOOK_COLUMNS - 1, IntroKey::LEFT);
    presses.insert(presses.end(), column, IntroKey::RIGHT);
    presses.insert(presses.end(), row, IntroKey::DOWN);
    presses.push_back(IntroKey::A);
    return presses;
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
    if (!language_list_presses(language)){
        return "The new game's language list has no such language.";
    }
    if (!look_picker_presses(choices.look)){
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
