/*  Story Step JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <set>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Pokemon_StoryStepJson.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  A step, parsed by its action among `actions`.
StoryStep read_story_step(const JsonValue& value, const StoryActions& actions){
    const JsonObject& root = value.to_object_throw();
    reject_unknown_keys(root, {"id", "why", "action", "args"});
    StoryStep result;
    result.name = root.get_string_throw("id");
    if (result.name.empty()){
        throw ParseException("empty step id");
    }
    if (const JsonValue* field = root.get_value("why")){
        result.why = field->to_string_throw();
    }
    const std::string& action = root.get_string_throw("action");
    result.action = action;
    const auto parser = actions.find(action);
    if (parser == actions.end()){
        throw ParseException(std::format("step {}: unknown action \"{}\"", result.name, action));
    }
    const JsonObject no_args;
    const JsonValue* args = root.get_value("args");
    try{
        result.run = parser->second(args != nullptr ? args->to_object_throw() : no_args);
    }catch (const ParseException& error){
        throw ParseException(std::format("step {}: {}", result.name, error.message()));
    }
    return result;
}


} // namespace


void reject_unknown_keys(const JsonObject& object, std::initializer_list<std::string_view> keys){
    for (const auto& [key, unused] : object){
        bool known = false;
        for (std::string_view allowed : keys){
            known |= key == allowed;
        }
        if (!known){
            throw ParseException(std::format("unknown field \"{}\"", key));
        }
    }
}

StoryCheckpoint read_story_checkpoint(const JsonValue& value, std::string_view source, const StoryActions& actions){
    try{
        const JsonObject& root = value.to_object_throw();
        reject_unknown_keys(root, {"id", "title", "start", "events", "metadata"});
        StoryCheckpoint result;
        result.id = root.get_string_throw("id");
        if (result.id.empty()){
            throw ParseException("empty checkpoint id");
        }
        result.title = root.get_string_throw("title");
        std::set<std::string> names;
        for (const JsonValue& event : root.get_array_throw("events")){
            StoryStep step = read_story_step(event, actions);
            if (!names.insert(step.name).second){
                throw ParseException(std::format("duplicate step id {}", step.name));
            }
            result.steps.push_back(std::move(step));
        }
        return result;
    }catch (const ParseException& error){
        throw ParseException(std::format("{}: {}", source, error.message()));
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
