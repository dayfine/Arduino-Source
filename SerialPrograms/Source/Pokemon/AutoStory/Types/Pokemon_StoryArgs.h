/*  Story Args
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A step's arguments (StoryStep::args): named flags, whole numbers and
 *  words that its action reads, e.g. a Mart step's {"map":
 *  "CeruleanCity_Mart", "budget": 3000}. Each action declares the ones it
 *  takes (StoryArgSpec); a game checks a step's args against them when it
 *  loads the step, so a typo fails at load instead of mid-run.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryArgs_H
#define PokemonAutomation_Pokemon_StoryArgs_H

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace PokemonAutomation{
namespace Pokemon{


//  One argument's value: a flag, a whole number or a word.
using StoryArgValue = std::variant<bool, int64_t, std::string>;

//  A step's arguments by name.
using StoryArgs = std::map<std::string, StoryArgValue, std::less<>>;


//  The number `key` holds; nothing if absent or not a number.
std::optional<int64_t> story_arg_number(const StoryArgs& args, std::string_view key);

//  The word `key` holds; nothing if absent or not a word.
std::optional<std::string_view> story_arg_text(const StoryArgs& args, std::string_view key);

//  Whether flag `key` is set; absent is unset.
bool story_arg_flag(const StoryArgs& args, std::string_view key);


//  The type an action declares for an argument.
enum class StoryArgType{
    FLAG,
    NUMBER,
    TEXT,
};

//  An argument an action takes.
struct StoryArgSpec{
    std::string_view key;
    StoryArgType type = StoryArgType::TEXT;
    bool required = false;
};

//  Why `args` don't fit `specs`: a key no spec names, a value of another
//  type, or a required one missing. Empty when they fit.
std::string story_args_error(const StoryArgs& args, std::span<const StoryArgSpec> specs);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
