/*  Story Action Spec
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Pokemon_StoryActionSpec.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


const StoryArgSpec* find_spec(const StoryActionSpec& action, std::string_view key){
    for (const StoryArgSpec& spec : action.args){
        if (spec.key == key){
            return &spec;
        }
    }
    return nullptr;
}


} // namespace


std::vector<std::string> story_args_errors(const StoryActionSpec& spec, const StoryActionArgs& args){
    std::vector<std::string> errors;
    for (const auto& [key, value] : args){
        const StoryArgSpec* arg = find_spec(spec, key);
        if (arg == nullptr){
            errors.emplace_back(std::format("unknown argument \"{}\"", key));
        }else if (story_arg_type(value) != arg->type){
            errors.emplace_back(std::format("argument \"{}\" has the wrong type", key));
        }
    }
    for (const StoryArgSpec& arg : spec.args){
        if (arg.required && args.find(arg.key) == args.end()){
            errors.emplace_back(std::format("missing argument \"{}\"{}", arg.key, arg.why.empty() ? "" : std::format(" ({})", arg.why)));
        }
    }
    return errors;
}

StoryActionArgs with_fallbacks(const StoryActionSpec& spec, StoryActionArgs args){
    for (const StoryArgSpec& arg : spec.args){
        if (arg.fallback){
            args.try_emplace(arg.key, *arg.fallback);
        }
    }
    return args;
}



} // namespace Pokemon
} // namespace PokemonAutomation
