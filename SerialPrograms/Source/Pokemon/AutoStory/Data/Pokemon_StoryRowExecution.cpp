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

constexpr std::string_view SHARED_FIELDS[] = {"save", "reload_when_chained", "dev_only"};

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

bool read_autosave(const JsonObject& execution, std::string_view where){
    const JsonValue* save = execution.get_value("save");
    if (save == nullptr){
        return false;
    }
    const std::string* text = save->to_string();
    if (text == nullptr || (*text != "autosave" && *text != "menu")){
        throw ParseException(std::format("{}: save is \"autosave\", \"menu\" or left out", where));
    }
    return *text == "autosave";
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

bool read_dev_only(const JsonObject& execution, std::string_view where){
    const JsonValue* dev_only = execution.get_value("dev_only");
    if (dev_only == nullptr){
        return false;
    }
    if (!is_true(*dev_only)){
        throw ParseException(std::format("{}: dev_only is true or left out", where));
    }
    return true;
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
        .autosave = read_autosave(execution, where),
        .reload = read_reload(execution, where),
        .dev_only = read_dev_only(execution, where),
    };
}

bool story_reloads_before(const StoryRows& rows, StoryRunPolicy::BoundaryReload boundary_reload, std::string_view id){
    if (rows.reload_always.contains(id)){
        return true;
    }
    return boundary_reload == StoryRunPolicy::BoundaryReload::AT_PLAN_BOUNDARIES && rows.reload_when_chained.contains(id);
}



} // namespace Pokemon
} // namespace PokemonAutomation
