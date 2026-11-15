/*  Story Row Execution
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon_StoryRowExecution.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

constexpr std::string_view SHARED_FIELDS[] = {"save", "reload_when_chained"};

bool is_known_field(std::string_view key, std::initializer_list<std::string_view> game_fields){
    for (std::string_view field : SHARED_FIELDS){
        if (key == field){
            return true;
        }
    }
    for (std::string_view field : game_fields){
        if (key == field){
            return true;
        }
    }
    return false;
}

//  Whether `value` is JSON true.
bool is_true(const JsonValue& value){
    return value.is_boolean() && value.to_boolean_default();
}

StoryRowSave read_save(const JsonObject& execution, std::string_view where){
    const JsonValue* save = execution.get_value("save");
    if (save == nullptr){
        return StoryRowSave::NONE;
    }
    const std::string* text = save->to_string();
    if (text != nullptr && *text == "autosave"){
        return StoryRowSave::AUTOSAVE;
    }
    if (text != nullptr && *text == "unverified"){
        return StoryRowSave::UNVERIFIED;
    }
    throw ParseException(std::format("{}: save is \"autosave\", \"unverified\" or left out", where));
}

StoryReload read_reload(const JsonObject& execution, std::string_view where){
    const JsonValue* reload = execution.get_value("reload_when_chained");
    if (reload == nullptr){
        return StoryReload::NEVER;
    }
    if (is_true(*reload)){
        return StoryReload::WHEN_CHAINED;
    }
    const std::string* text = reload->to_string();
    if (text != nullptr && *text == "always"){
        return StoryReload::ALWAYS;
    }
    throw ParseException(std::format("{}: reload_when_chained is true, \"always\" or left out", where));
}

} // namespace


StoryRowExecution read_story_row_execution(
    const JsonObject& execution, std::string_view where, std::initializer_list<std::string_view> game_fields
){
    for (const auto& [key, value] : execution){
        if (!is_known_field(key, game_fields)){
            throw ParseException(std::format("{}: unknown execution field {}", where, key));
        }
    }
    return StoryRowExecution{
        .save = read_save(execution, where),
        .reload = read_reload(execution, where),
    };
}




} // namespace Pokemon
} // namespace PokemonAutomation
