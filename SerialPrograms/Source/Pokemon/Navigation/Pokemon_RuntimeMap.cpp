/*  Versioned Runtime Map Decoder
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <array>
#include <limits>
#include <set>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon_Grid.h"
#include "Pokemon_RuntimeMap.h"

namespace PokemonAutomation{
namespace Pokemon{
namespace{

enum class WireFormat{ LEGACY, COMPACT };

const JsonObject& object_at(const JsonValue& value, const std::string& where){
    const JsonObject* object = value.to_object();
    if (!object){
        throw ParseException(where + " is not a JSON object.");
    }
    return *object;
}

const JsonArray& array_at(const JsonValue& value, const std::string& where){
    const JsonArray* array = value.to_array();
    if (!array){
        throw ParseException(where + " must be an array.");
    }
    return *array;
}

JsonObject copy_without(const JsonObject& object, const std::set<std::string>& omit){
    JsonObject result;
    for (const auto& [key, value] : object){
        if (!omit.contains(key)){
            result[key] = value.clone();
        }
    }
    return result;
}

int coordinate(const JsonValue* value, const std::string& where){
    if (!value){
        throw ParseException(where + " is missing.");
    }
    if (!value->is_integer()){
        throw ParseException(where + " is not an integer.");
    }
    const int64_t number = value->to_integer_default();
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()){
        throw ParseException(where + " does not fit a native coordinate.");
    }
    return (int)number;
}

TilePos pair_at(const JsonObject& object, const std::array<std::string, 3>& keys, WireFormat format, const std::string& where){
    if (format == WireFormat::LEGACY){
        if (object.get_value(keys[2])){
            throw ParseException(where + keys[2] + " is not a v1 coordinate field.");
        }
        return {coordinate(object.get_value(keys[0]), where + keys[0]), coordinate(object.get_value(keys[1]), where + keys[1])};
    }
    const JsonValue* value = object.get_value(keys[2]);
    if (object.get_value(keys[0]) || object.get_value(keys[1]) || !value){
        throw ParseException(where + keys[2] + " requires only the v2 coordinate pair.");
    }
    const JsonArray& pair = array_at(*value, where + keys[2]);
    if (pair.size() != 2){
        throw ParseException(where + keys[2] + " requires exactly two integers.");
    }
    return {coordinate(&pair[0], where + keys[2] + "[0]"), coordinate(&pair[1], where + keys[2] + "[1]")};
}

JsonValue normalize_wander(const JsonValue& value, WireFormat format, const std::string& where){
    const JsonObject& source = object_at(value, where);
    const TilePos low = pair_at(source, {"x0", "y0", "min"}, format, where);
    const TilePos high = pair_at(source, {"x1", "y1", "max"}, format, where);
    if (format == WireFormat::COMPACT && (low.x > high.x || low.y > high.y)){
        throw ParseException(where + "must have inclusive min <= max bounds.");
    }
    JsonObject result = copy_without(source, {"x0", "y0", "x1", "y1", "min", "max"});
    result["x0"] = low.x;
    result["y0"] = low.y;
    result["x1"] = high.x;
    result["y1"] = high.y;
    return result;
}

JsonObject normalize_record(const JsonValue& value, WireFormat format, const std::string& where){
    const JsonObject& source = object_at(value, where);
    const TilePos pos = pair_at(source, {"x", "y", "coord"}, format, where);
    JsonObject result = copy_without(source, {"x", "y", "coord"});
    result["x"] = pos.x;
    result["y"] = pos.y;
    return result;
}

void append_objects(JsonArray& output, const JsonArray& input, WireFormat format, const std::string& group, const std::string& where){
    for (size_t i = 0; i < input.size(); i++){
        const std::string path = where + group + "[" + std::to_string(i) + "].";
        JsonObject record = normalize_record(input[i], format, path);
        if (format == WireFormat::COMPACT){
            if (record.get_value("kind")){
                throw ParseException(path + "kind is redundant in v2 typed arrays.");
            }
            record["kind"] = group == "npcs" ? "npc" : group == "signs" ? "sign" : "item";
        }
        if (group == "interactables"){
            const JsonObject* interaction = record.get_object("interaction");
            const std::string* action = interaction ? interaction->get_string("kind") : nullptr;
            if (!action || action->empty() || record.get_value("item") || record.get_value("quantity")){
                throw ParseException(path + "requires an interaction without an item reward.");
            }
        }else if (group == "items" && record.get_value("interaction")){
            throw ParseException(path + "interactions belong in interactables.");
        }
        if (const JsonValue* wander = record.get_value("wander")){
            record["wander"] = normalize_wander(*wander, format, path + "wander.");
        }
        output.push_back(std::move(record));
    }
}

WireFormat read_format(const JsonObject& root, const std::string& where){
    const JsonValue* field = root.get_value("format");
    if (!field){
        return WireFormat::LEGACY;
    }
    const std::string* format = field->to_string();
    if (format && *format == "pokesim-runtime-map/1"){
        return WireFormat::LEGACY;
    }
    if (format && *format == "pokesim-runtime-map/2"){
        return WireFormat::COMPACT;
    }
    throw ParseException(where + "format is unsupported.");
}

void normalize_objects(JsonObject& output, const JsonObject& root, WireFormat format, const std::string& where){
    JsonArray objects;
    const std::array<std::string, 4> groups{"npcs", "items", "signs", "interactables"};
    if (format == WireFormat::LEGACY){
        for (const std::string& group : groups){
            if (root.get_value(group)){
                throw ParseException(where + group + " is not a v1 object section.");
            }
        }
        if (const JsonValue* value = root.get_value("objects")){
            append_objects(objects, array_at(*value, where + "objects"), format, "objects", where);
            output["objects"] = std::move(objects);
        }
        return;
    }
    if (root.get_value("objects")){
        throw ParseException(where + "objects cannot be combined with v2 typed arrays.");
    }
    bool present = false;
    for (const std::string& group : groups){
        if (const JsonValue* value = root.get_value(group)){
            present = true;
            append_objects(objects, array_at(*value, where + group), format, group, where);
        }
    }
    if (present){
        output["objects"] = std::move(objects);
    }
}

} // namespace

JsonValue normalize_runtime_map(const JsonValue& json, std::string_view source_name){
    const std::string where = std::string(source_name) + ": ";
    const JsonObject& root = object_at(json, where);
    const WireFormat format = read_format(root, where);
    JsonObject output = copy_without(root, {"format", "objects", "npcs", "items", "signs", "interactables", "warps", "patches", "coord_events"});
    normalize_objects(output, root, format, where);
    for (const std::string section : {"warps", "patches", "coord_events"}){
        if (const JsonValue* field = root.get_value(section)){
            const JsonArray& input = array_at(*field, where + section);
            JsonArray records;
            for (size_t i = 0; i < input.size(); i++){
                records.push_back(normalize_record(input[i], format, where + section + "[" + std::to_string(i) + "]."));
            }
            output[section] = std::move(records);
        }
    }
    return output;
}

} // namespace Pokemon
} // namespace PokemonAutomation
