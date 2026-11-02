/*  BDSP AutoStory - Coord events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP_AutoStory_CoordEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::TilePos;


namespace{

int read_int(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonValue* value = object.get_value(key);
    if (value == nullptr || !value->is_integer()){
        throw ParseException(where + key + " is missing or not an integer.");
    }
    return (int)value->to_integer_default();
}

std::string read_string(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonValue* value = object.get_value(key);
    const std::string* text = value == nullptr ? nullptr : value->to_string();
    if (text == nullptr){
        throw ParseException(where + key + " is missing or not a string.");
    }
    return *text;
}

CoordEvent read_event(const JsonObject& object, const std::string& where){
    CoordEvent event{
        .name = read_string(object, "name", where),
        .tiles = {},
        .var = read_string(object, "var", where),
        .value = read_int(object, "value", where),
        .source = read_string(object, "source", where),
    };
    const int x = read_int(object, "x", where);
    const int y = read_int(object, "y", where);
    const int width = read_int(object, "width", where);
    const int length = read_int(object, "length", where);
    if (width < 1 || length < 1){
        throw ParseException(where + "width and length must be at least 1.");
    }
    for (int row = y; row < y + length; row++){
        for (int col = x; col < x + width; col++){
            event.tiles.push_back({col, row});
        }
    }
    return event;
}

std::vector<CoordEvent> read_events(const std::string& name, const std::string& json){
    std::vector<CoordEvent> events;
    const JsonValue root = parse_json(json);
    const JsonObject* object = root.to_object();
    const JsonValue* list = object == nullptr ? nullptr : object->get_value("coord_events");
    if (list == nullptr){
        return events;
    }
    const JsonArray* array = list->to_array();
    if (array == nullptr){
        throw ParseException(name + ".json: coord_events is not an array.");
    }
    for (size_t c = 0; c < array->size(); c++){
        const std::string where = std::format("{}.json: coord_events[{}].", name, c);
        const JsonObject* item = (*array)[c].to_object();
        if (item == nullptr){
            throw ParseException(where + " is not an object.");
        }
        events.push_back(read_event(*item, where));
    }
    return events;
}

}


const std::vector<CoordEvent>& story_coord_events(std::string_view map){
    static const std::map<std::string, std::vector<CoordEvent>, std::less<>> by_map = []{
        std::map<std::string, std::vector<CoordEvent>, std::less<>> out;
        for (const auto& [name, json] : bdsp_map_json()){
            out.emplace(name, read_events(name, json));
        }
        return out;
    }();
    if (auto iter = by_map.find(map); iter != by_map.end()){
        return iter->second;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No BDSP story map named \"{}\".", map));
}



}
}
}
