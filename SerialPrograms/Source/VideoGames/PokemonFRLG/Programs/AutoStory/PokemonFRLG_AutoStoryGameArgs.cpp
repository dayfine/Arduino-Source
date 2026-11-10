/*  AutoStory Game Args
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_AutoStoryGameArgs.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


Pokemon::StoryArgSpec required_arg(std::string key, Pokemon::StoryArgType type, std::string why){
    return Pokemon::StoryArgSpec{.key = std::move(key), .type = type, .required = true, .fallback = std::nullopt, .why = std::move(why)};
}

Pokemon::StoryArgSpec optional_arg(std::string key, Pokemon::StoryArgType type, Pokemon::StoryArgValue fallback, std::string why){
    return Pokemon::StoryArgSpec{.key = std::move(key), .type = type, .required = false, .fallback = std::move(fallback), .why = std::move(why)};
}

Pokemon::StoryArgSpec absent_arg(std::string key, Pokemon::StoryArgType type, std::string why){
    return Pokemon::StoryArgSpec{.key = std::move(key), .type = type, .required = false, .fallback = std::nullopt, .why = std::move(why)};
}

std::optional<size_t> number_arg(const Pokemon::StoryActionArgs& args, std::string_view key, size_t min, size_t max){
    const int64_t* value = game_arg<int64_t>(args, key);
    if (value == nullptr || *value < (int64_t)min || *value > (int64_t)max){
        return std::nullopt;
    }
    return (size_t)*value;
}

std::optional<size_t> counted_arg(const Pokemon::StoryActionArgs& args, std::string_view key, size_t max){
    const std::optional<size_t> value = number_arg(args, key, 1, max);
    if (!value){
        return std::nullopt;
    }
    return *value - 1;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
