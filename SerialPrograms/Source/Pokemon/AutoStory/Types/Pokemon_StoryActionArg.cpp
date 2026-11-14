/*  Story Action Arg
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryActionArg.h"

namespace PokemonAutomation{
namespace Pokemon{


StoryArgType story_arg_type(const StoryArgValue& value){
    return (StoryArgType)value.index();
}



} // namespace Pokemon
} // namespace PokemonAutomation
