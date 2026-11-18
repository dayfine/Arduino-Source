/*  AutoStory Game Args
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Declaring and reading the args of FRLG's own GAME actions (a PC
 *  transfer, a capture, a Mart, a TEACH...): each declares its args as a
 *  Pokemon::StoryActionSpec, the story file gives them, the reader fills in
 *  the fallbacks, and the step reads them back into its plan.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryGameArgs_H
#define PokemonAutomation_PokemonFRLG_AutoStoryGameArgs_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include "Pokemon/AutoStory/Pokemon_MapUtil.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  An arg the action needs.
Pokemon::StoryArgSpec required_arg(std::string key, Pokemon::StoryArgType type, std::string why);
//  An arg read as `fallback` when the step leaves it out.
Pokemon::StoryArgSpec optional_arg(std::string key, Pokemon::StoryArgType type, Pokemon::StoryArgValue fallback, std::string why);
//  An arg the step may leave out, with no fallback: absent means "none".
Pokemon::StoryArgSpec absent_arg(std::string key, Pokemon::StoryArgType type, std::string why);

//  `args`' value of `key`, or null when it is missing or of another type.
template <typename Type>
const Type* game_arg(const Pokemon::StoryActionArgs& args, std::string_view key);

template <typename Type>
const Type* game_arg(const Pokemon::StoryActionArgs& args, std::string_view key){
    return Pokemon::value_if_at_it<Type>(args, args.find(key));
}

//  The INTEGER arg `key` when it is from `min` to `max`; nothing if it is
//  missing, of another type or out of range.
std::optional<size_t> number_arg(const Pokemon::StoryActionArgs& args, std::string_view key, size_t min, size_t max);

//  The INTEGER arg `key`, a slot or box counted from 1 to `max` as the game
//  shows it, counted from 0; nothing if it is missing or out of range.
std::optional<size_t> counted_arg(const Pokemon::StoryActionArgs& args, std::string_view key, size_t max);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
