/*  Story Action Arg
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryActionArg.h"

namespace PokemonAutomation{
namespace Pokemon{


StoryArgType story_arg_type(const StoryArgValue& value){
    //  The alternatives are declared in StoryArgType's order. A variant left
    //  valueless by an exception would give variant_npos; none is kept.
    return static_cast<StoryArgType>(value.index());
}



} // namespace Pokemon
} // namespace PokemonAutomation
