/*  Story Action Arg
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story step's typed arguments.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryActionArg_H
#define PokemonAutomation_Pokemon_StoryActionArg_H

#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <any>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_MapUtil.h"
#include "Pokemon_StoryRegisteredArg.h"

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
    REGISTERED,
};

//  One argument's value; the index is its StoryArgType.
using StoryArgValue = std::variant<
    bool, int64_t, std::string,             //  BOOLEAN, INTEGER, WORD
    std::vector<std::string>,               //  WORDS
    std::vector<std::vector<int64_t>>,      //  INTEGER_ROWS: rows of any length, [[12, 7], [3, 9, 1], []]
    std::vector<StoryTimedLeg>,             //  TIMED_LEGS: [turn degrees, ms] pairs, [[0, 4500], [90, 1200]]
    StoryRegisteredArgValue                 //  REGISTERED: read by the arg's StoryArgKind
>;
static_assert(std::variant_size_v<StoryArgValue> == (size_t)StoryArgType::REGISTERED + 1, "one StoryArgType per StoryArgValue type");

//  A step's arguments by key.
using StoryActionArgs = std::map<std::string, StoryArgValue, std::less<>>;

//  The type `value` holds.
StoryArgType story_arg_type(const StoryArgValue& value);

//  The value of arg `key`, which the action's spec requires or gives a
//  fallback. Throws InternalProgramError when it is missing or mistyped.
template <typename Type>
const Type& story_arg(const StoryActionArgs& args, std::string_view key);

//  story_arg() for a registered arg holding a `Type`. Throws
//  InternalProgramError when it is missing or holds another type.
template <typename Type>
const Type& registered_arg(const StoryActionArgs& args, std::string_view key);



template <typename Type>
const Type& story_arg(const StoryActionArgs& args, std::string_view key){
    const Type* value = value_if_at_it<Type>(args, args.find(key));
    if (value == nullptr){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("Missing or mistyped story arg: {}", key));
    }
    return *value;
}

template <typename Type>
const Type& registered_arg(const StoryActionArgs& args, std::string_view key){
    const StoryRegisteredArgValue& held = story_arg<StoryRegisteredArgValue>(args, key);
    const Type* typed = std::any_cast<Type>(&held.value);
    if (typed == nullptr){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("Story arg of another registered kind: {}", key));
    }
    return *typed;
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
