/*  Story Registered Arg
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A value of an arg kind a game registers; the core only carries it.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRegisteredArg_H
#define PokemonAutomation_Pokemon_StoryRegisteredArg_H

#include <any>
#include <utility>

namespace PokemonAutomation{
namespace Pokemon{


//  A registered arg as a step holds it: the game's value, and how two
//  values of its type compare. Empty values are equal.
struct StoryRegisteredArgValue{
    std::any value = {};
    bool (*equals)(const std::any& a, const std::any& b) = nullptr;

    bool operator==(const StoryRegisteredArgValue& other) const;
};

//  `value` held as a registered arg; `Type` needs operator==.
template <typename Type>
StoryRegisteredArgValue make_registered_arg(Type value);



template <typename Type>
StoryRegisteredArgValue make_registered_arg(Type value){
    return StoryRegisteredArgValue{
        .value = std::any(std::move(value)),
        .equals = [](const std::any& a, const std::any& b){
            const Type* left = std::any_cast<Type>(&a);
            const Type* right = std::any_cast<Type>(&b);
            return left != nullptr && right != nullptr && *left == *right;
        },
    };
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
