/*  Story Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The shared engine's built-in actions (a route, fixed legs, a battle, a
 *  shop...) with the args each takes, and reading a step's args. A game
 *  adds its own actions (GAME steps and its other ways of doing a kind) to
 *  these when it loads its tables.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryActions_H
#define PokemonAutomation_Pokemon_StoryActions_H

#include <span>
#include <string>
#include <string_view>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_MapUtil.h"
#include "Types/Pokemon_StoryActionArg.h"
#include "Types/Pokemon_StoryActionSpec.h"
#include "Types/Pokemon_StoryStepKind.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The built-in actions: each kind's usual way. A kind's first one is its
//  default, the action of a step that names none.
//      route   ROUTE   target GRID_POSE (required), fight_wild BOOLEAN (false)
//      ready   READY   target GRID_POSE (required): the nurse
//      legs    LEGS    legs LEGS (required), fight_wild BOOLEAN (false)
//      face    STEP    direction DIRECTION (required)
//      answer  ANSWER  yes BOOLEAN (true)
//      fight   BATTLE  may_lose BOOLEAN (false)
//      train   TRAIN   battles INTEGER (required)
//      shop    SHOP    items ITEMS (required)
//  Other kinds (TALK, MASH, HEAL, SAVE, ...) take no args.
std::span<const StoryActionSpec> builtin_story_actions();

//  The action of `kind` named `name` among `actions`; for an empty name,
//  the kind's first. Null when there is none.
const StoryActionSpec* find_story_action(std::span<const StoryActionSpec> actions, StoryStepKind kind, std::string_view name);

//  The value of arg `key`, which the step's action requires or gives a
//  fallback, so the loaded step has it. Throws InternalProgramError when it
//  is missing or of another type: a step built without its action's spec.
template <typename Type>
const Type& story_arg(const StoryActionArgs& args, std::string_view key);


template <typename Type>
const Type& story_arg(const StoryActionArgs& args, std::string_view key){
    const Type* value = value_if_at_it<Type>(args, args.find(key));
    if (value == nullptr){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Missing or mistyped story arg: " + std::string(key));
    }
    return *value;
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
