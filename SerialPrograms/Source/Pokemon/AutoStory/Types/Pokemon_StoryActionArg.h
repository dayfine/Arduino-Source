/*  Story Action Arg
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A step's action and its arguments (StoryStep::action, StoryStep::args),
 *  e.g. a Mart step's {"map": "CeruleanCity_Mart", "budget": 3000}. Each
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
#include <functional>
#include <map>
#include <string>
#include <variant>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon_GridPose.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A straight leg of a fixed walk.
struct StoryLeg{
    Direction direction = Direction::NORTH;
    int tiles = 0;

    bool operator==(const StoryLeg& other) const = default;
};

//  An item and how many to get.
struct StoryItem{
    std::string item;
    size_t quantity = 0;

    bool operator==(const StoryItem& other) const = default;
};


//  The type an action declares for an argument, in StoryArgValue's order.
enum class StoryArgType{
    BOOLEAN,
    INTEGER,
    WORD,
    DIRECTION,
    GRID_POSE,
    LEGS,
    WORDS,
    ITEMS,
};

//  One argument's value; the index is its StoryArgType.
using StoryArgValue = std::variant<
    bool, int64_t, std::string,             //  BOOLEAN, INTEGER, WORD
    Direction, GridPose,                    //  DIRECTION, GRID_POSE
    std::vector<StoryLeg>,                  //  LEGS
    std::vector<std::string>,               //  WORDS
    std::vector<StoryItem>                  //  ITEMS
>;

//  A step's arguments by key.
using StoryActionArgs = std::map<std::string, StoryArgValue, std::less<>>;

//  The type `value` holds.
StoryArgType story_arg_type(const StoryArgValue& value);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
