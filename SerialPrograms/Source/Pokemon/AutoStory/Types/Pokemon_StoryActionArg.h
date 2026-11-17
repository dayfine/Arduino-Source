/*  Story Action Arg
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A step's action and its arguments (StoryStep::action, StoryStep::args),
 *  e.g. a walk's {"legs": [[0, 4500], [90, 10000]], "end": "fade_white"}. Each
 *  action declares the arguments it takes (Pokemon_StoryActionSpec.h), with their
 *  types and defaults; the JSON reader (Data/Pokemon_StoryStepJson.h)
 *  parses a step's args against them and fills in the defaults, so a typo
 *  fails at load instead of mid-run and nothing after it reads JSON.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryActionArg_H
#define PokemonAutomation_Pokemon_StoryActionArg_H

#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_MapUtil.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A leg of a timed walk in a game with no tiles: turn `turn` degrees
//  (clockwise, from the way the player faces), then walk for `walk`.
struct StoryTimedLeg{
    int turn = 0;
    Milliseconds walk = Milliseconds(0);

    bool operator==(const StoryTimedLeg& other) const = default;
};


//  The type an action declares for an argument, in StoryArgValue's order.
enum class StoryArgType{
    BOOLEAN,
    INTEGER,
    WORD,
    WORDS,
    INTEGER_ROWS,
    TIMED_LEGS,
};

//  One argument's value; the index is its StoryArgType.
using StoryArgValue = std::variant<
    bool, int64_t, std::string,             //  BOOLEAN, INTEGER, WORD
    std::vector<std::string>,               //  WORDS
    std::vector<std::vector<int64_t>>,      //  INTEGER_ROWS: rows of any length, [[12, 7], [3, 9, 1], []]
    std::vector<StoryTimedLeg>              //  TIMED_LEGS: [turn degrees, ms] pairs, [[0, 4500], [90, 1200]]
>;
static_assert(std::variant_size_v<StoryArgValue> == (size_t)StoryArgType::TIMED_LEGS + 1, "one StoryArgType per StoryArgValue type");

//  A step's arguments by key.
using StoryActionArgs = std::map<std::string, StoryArgValue, std::less<>>;

//  The type `value` holds.
StoryArgType story_arg_type(const StoryArgValue& value);

//  The value of arg `key`, which the step's action requires or gives a
//  fallback, so the loaded step has it. Throws InternalProgramError when it
//  is missing or of another type: a step built without its action's spec.
template <typename Type>
const Type& story_arg(const StoryActionArgs& args, std::string_view key);



template <typename Type>
const Type& story_arg(const StoryActionArgs& args, std::string_view key){
    const Type* value = value_if_at_it<Type>(args, args.find(key));
    if (value == nullptr){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("Missing or mistyped story arg: {}", key));
    }
    return *value;
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
