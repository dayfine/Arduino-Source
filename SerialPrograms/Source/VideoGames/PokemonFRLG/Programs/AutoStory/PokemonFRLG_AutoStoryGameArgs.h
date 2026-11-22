/*  AutoStory Game Args
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading typed S10 args of FRLG's story steps.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryGameArgs_H
#define PokemonAutomation_PokemonFRLG_AutoStoryGameArgs_H

#include <cstddef>
#include <optional>
#include <string_view>
#include "Pokemon/AutoStory/Pokemon_MapUtil.h"
#include "PokemonFRLG_AutoStoryStepTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  `args`' value of `key`, or null when it is missing or of another type.
template <typename Type>
const Type* game_arg(const FrlgArgMap& args, std::string_view key){
    return Pokemon::value_if_at_it<Type>(args, args.find(key));
}

//  The INTEGER arg `key` when it is from `min` to `max`; nothing if it is
//  missing, of another type or out of range.
std::optional<size_t> number_arg(const FrlgArgMap& args, std::string_view key, size_t min, size_t max);

//  The INTEGER arg `key`, a slot or box counted from 1 to `max` as the game
//  shows it, counted from 0; nothing if it is missing or out of range.
std::optional<size_t> counted_arg(const FrlgArgMap& args, std::string_view key, size_t max);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
