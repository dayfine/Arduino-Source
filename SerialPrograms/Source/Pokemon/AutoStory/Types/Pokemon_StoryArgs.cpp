/*  Story Args
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_StoryArgs.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


template <typename Type>
const Type* find_arg(const StoryArgs& args, std::string_view key){
    const auto iter = args.find(key);
    return iter == args.end() ? nullptr : std::get_if<Type>(&iter->second);
}

bool has_type(const StoryArgValue& value, StoryArgType type){
    switch (type){
    case StoryArgType::FLAG:
        return std::holds_alternative<bool>(value);
    case StoryArgType::NUMBER:
        return std::holds_alternative<int64_t>(value);
    case StoryArgType::TEXT:
        return std::holds_alternative<std::string>(value);
    }
    return false;
}

const StoryArgSpec* find_spec(std::span<const StoryArgSpec> specs, std::string_view key){
    for (const StoryArgSpec& spec : specs){
        if (spec.key == key){
            return &spec;
        }
    }
    return nullptr;
}


} // namespace


std::optional<int64_t> story_arg_number(const StoryArgs& args, std::string_view key){
    const int64_t* value = find_arg<int64_t>(args, key);
    return value ? std::optional<int64_t>(*value) : std::nullopt;
}

std::optional<std::string_view> story_arg_text(const StoryArgs& args, std::string_view key){
    const std::string* value = find_arg<std::string>(args, key);
    return value ? std::optional<std::string_view>(*value) : std::nullopt;
}

bool story_arg_flag(const StoryArgs& args, std::string_view key){
    const bool* value = find_arg<bool>(args, key);
    return value != nullptr && *value;
}

std::string story_args_error(const StoryArgs& args, std::span<const StoryArgSpec> specs){
    for (const auto& [key, value] : args){
        const StoryArgSpec* spec = find_spec(specs, key);
        if (spec == nullptr){
            return std::format("unknown argument \"{}\"", key);
        }
        if (!has_type(value, spec->type)){
            return std::format("argument \"{}\" has the wrong type", key);
        }
    }
    for (const StoryArgSpec& spec : specs){
        if (spec.required && args.find(spec.key) == args.end()){
            return std::format("missing argument \"{}\"", spec.key);
        }
    }
    return "";
}



} // namespace Pokemon
} // namespace PokemonAutomation
