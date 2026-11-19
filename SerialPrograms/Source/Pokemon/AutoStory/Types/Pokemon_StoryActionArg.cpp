/*  Story Action Arg
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryActionArg.h"

namespace PokemonAutomation{
namespace Pokemon{


bool StoryRegisteredArgValue::operator==(const StoryRegisteredArgValue& other) const{
    if (!value.has_value() || !other.value.has_value()){
        return value.has_value() == other.value.has_value();
    }
    return equals != nullptr && equals(value, other.value);
}

StoryArgType story_arg_type(const StoryArgValue& value){
    //  The alternatives are declared in StoryArgType's order.
    return static_cast<StoryArgType>(value.index());
}



} // namespace Pokemon
} // namespace PokemonAutomation
