/*  PokemonLZA AutoStory - New game intro
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_AutoStoryIntro.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::optional<std::vector<IntroKey>> language_list_presses(Language){
    return std::nullopt;
}

std::optional<std::vector<IntroKey>> look_picker_presses(size_t){
    return std::nullopt;
}

std::optional<NameKeys> name_keys(std::string_view, bool){
    return std::nullopt;
}

std::string new_game_problem(Language, const NewGameChoices&){
    return "TODO";
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
