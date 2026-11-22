/*  AutoStory Game Args
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include "PokemonFRLG_AutoStoryGameArgs.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::optional<size_t> number_arg(const FrlgArgMap& args, std::string_view key, size_t min, size_t max){
    const int64_t* value = game_arg<int64_t>(args, key);
    if (value == nullptr || *value < static_cast<int64_t>(min) || *value > static_cast<int64_t>(max)){
        return std::nullopt;
    }
    return static_cast<size_t>(*value);
}

std::optional<size_t> counted_arg(const FrlgArgMap& args, std::string_view key, size_t max){
    const std::optional<size_t> number = number_arg(args, key, 1, max);
    return number ? std::optional<size_t>(*number - 1) : std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
