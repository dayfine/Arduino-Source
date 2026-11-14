/*  Versioned Runtime Map Decoder
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <array>
#include <format>
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
        throw ParseException(std::format("{} is not a JSON object.", where));
    }
    return *object;
}

const JsonArray& array_at(const JsonValue& value, const std::string& where){
    const JsonArray* array = value.to_array();
    if (!array){
        throw ParseException(std::format("{} must be an array.", where));
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
        throw ParseException(std::format("{} is missing.", where));
    }
    if (!value->is_integer()){
        throw ParseException(std::format("{} is not an integer.", where));
    }
    const int64_t number = value->to_integer_default();
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()){
        throw ParseException(std::format("{} does not fit a native coordinate.", where));
    }
    return (int)number;
}

TilePos pair_at(const JsonObject& object, const std::array<std::string, 3>& keys, WireFormat format, const std::string& where){
    if (format == WireFormat::LEGACY){
        if (object.get_value(keys[2])){
            throw ParseException(std::format("{}{} is not a v1 coordinate field.", where, keys[2]));
        }
        return {coordinate(object.get_value(keys[0]), where + keys[0]), coordinate(object.get_value(keys[1]), where + keys[1])};
    }
    const JsonValue* value = object.get_value(keys[2]);
    if (object.get_value(keys[0]) || object.get_value(keys[1]) || !value){
        throw ParseException(std::format("{}{} requires only the v2 coordinate pair.", where, keys[2]));
    }
    const std::string pair_path = where + keys[2];
    const JsonArray& pair = array_at(*value, pair_path);
    if (pair.size() != 2){
        throw ParseException(std::format("{}{} requires exactly two integers.", where, keys[2]));
    }
    return {coordinate(&pair[0], std::format("{}{}[0]", where, keys[2])), coordinate(&pair[1], std::format("{}{}[1]", where, keys[2]))};
}

JsonValue normalize_wander(const JsonValue& value, WireFormat format, const std::string& where){
    const JsonObject& source = object_at(value, where);
    const TilePos low = pair_at(source, {"x0", "y0", "min"}, format, where);
    const TilePos high = pair_at(source, {"x1", "y1", "max"}, format, where);
    if (format == WireFormat::COMPACT && (low.x > high.x || low.y > high.y)){
        throw ParseException(std::format("{}must have inclusive min <= max bounds.", where));
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

//  Expand compact pickup defaults without changing authored names or counts.
JsonObject normalize_pickup(JsonObject record, std::string_view where){
    const std::string* item = record.get_string("item");
    if (!item || item->empty()){
        throw ParseException(std::format("{}item must be a nonempty string.", where));
    }
    if (const JsonValue* event = record.get_value("source_event")){
        const int index = coordinate(event, std::format("{}source_event", where));
        if (index < 0){
            throw ParseException(std::format("{}source_event must be nonnegative.", where));
        }
        if (!record.get_value("name")){
            record["name"] = std::format("item_{}", index);
        }
    }else if (!record.get_value("name")){
        throw ParseException(std::format("{}unnamed pickup requires source_event.", where));
    }
    if (const JsonValue* quantity = record.get_value("quantity")){
        if (!quantity->is_integer() || quantity->to_integer_default() <= 0){
            throw ParseException(std::format("{}quantity must be a positive integer.", where));
        }
    }else{
        record["quantity"] = 1;
    }
    return record;
}

void append_objects(const JsonArray& input, WireFormat format, const std::string& group, const std::string& where, JsonArray& output){
    for (size_t i = 0; i < input.size(); i++){
        const std::string path = std::format("{}{}[{}].", where, group, i);
        JsonObject record = normalize_record(input[i], format, path);
        if (format == WireFormat::COMPACT){
            if (record.get_value("kind")){
                throw ParseException(std::format("{}kind is redundant in v2 typed arrays.", path));
            }
            record["kind"] = group == "npcs" ? "npc" : group == "signs" ? "sign" : "item";
        }
        if (group == "interactables"){
            const JsonObject* interaction = record.get_object("interaction");
            const std::string* action = interaction ? interaction->get_string("kind") : nullptr;
            if (!action || action->empty() || record.get_value("item") || record.get_value("quantity")){
                throw ParseException(std::format("{}requires an interaction without an item reward.", path));
            }
        }else if (group == "items" && record.get_value("interaction")){
            throw ParseException(std::format("{}interactions belong in interactables.", path));
        }
        if (group == "items" && (record.get_value("item") || record.get_value("quantity"))){
            record = normalize_pickup(std::move(record), path);
        }
        if (const JsonValue* wander = record.get_value("wander")){
            record["wander"] = normalize_wander(*wander, format, std::format("{}wander.", path));
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
    throw ParseException(std::format("{}format is unsupported.", where));
}

void normalize_objects(const JsonObject& root, WireFormat format, const std::string& where, JsonObject& output){
    JsonArray objects;
    const std::array<std::string, 4> groups{"npcs", "items", "signs", "interactables"};
    if (format == WireFormat::LEGACY){
        for (const std::string& group : groups){
            if (root.get_value(group)){
                throw ParseException(std::format("{}{} is not a v1 object section.", where, group));
            }
        }
        if (const JsonValue* value = root.get_value("objects")){
            append_objects(array_at(*value, std::format("{}objects", where)), format, "objects", where, objects);
            output["objects"] = std::move(objects);
        }
        return;
    }
    if (root.get_value("objects")){
        throw ParseException(std::format("{}objects cannot be combined with v2 typed arrays.", where));
    }
    bool present = false;
    for (const std::string& group : groups){
        if (const JsonValue* value = root.get_value(group)){
            present = true;
            append_objects(array_at(*value, where + group), format, group, where, objects);
        }
    }
    if (present){
        output["objects"] = std::move(objects);
    }
}

} // namespace

JsonValue normalize_runtime_map(const JsonValue& json, std::string_view source_name){
    const std::string where = std::format("{}: ", source_name);
    const JsonObject& root = object_at(json, where);
    const WireFormat format = read_format(root, where);
    JsonObject output = copy_without(root, {"format", "objects", "npcs", "items", "signs", "interactables", "warps", "patches", "coord_events"});
    normalize_objects(root, format, where, output);
    for (const std::string section : {"warps", "patches", "coord_events"}){
        if (const JsonValue* field = root.get_value(section)){
            const std::string section_path = where + section;
            const JsonArray& input = array_at(*field, section_path);
            JsonArray records;
            for (size_t i = 0; i < input.size(); i++){
                records.push_back(normalize_record(input[i], format, std::format("{}{}[{}].", where, section, i)));
            }
            output[section] = std::move(records);
        }
    }
    return output;
}

} // namespace Pokemon
} // namespace PokemonAutomation
