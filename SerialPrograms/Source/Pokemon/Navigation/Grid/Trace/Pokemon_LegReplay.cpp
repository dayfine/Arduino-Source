/*  Leg Replay
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <stdexcept>
#include "Common/Cpp/EnumStringMap.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon_LegReplay.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

[[noreturn]] void bad(const std::string& name, const std::string& what){
    throw std::runtime_error(std::format("console failure {}: {}", name, what));
}

Direction direction_of(const std::string& name, const std::string& text){
    for (const Direction direction : ALL_DIRECTIONS){
        if (direction_name(direction) == text){
            return direction;
        }
    }
    bad(name, std::format("unknown direction \"{}\"", text));
}

TilePos tile_of(const std::string& name, const JsonArray& array){
    if (array.size() != 2){
        bad(name, "a tile is [x, y]");
    }
    return {(int)array[0].to_integer_throw(), (int)array[1].to_integer_throw()};
}

ReplayFade fade_of(const std::string& name, const std::string& text){
    if (text == "unseen"){
        return ReplayFade::UNSEEN;
    }
    if (text == "black"){
        return ReplayFade::BLACK;
    }
    if (text == "dim"){
        return ReplayFade::DIM;
    }
    if (text == "white"){
        return ReplayFade::WHITE;
    }
    bad(name, std::format("unknown fade \"{}\"", text));
}

ReplayFault fault_of(const std::string& name, const JsonObject& json){
    const std::string kind = json.get_string_throw("kind");
    ReplayFault fault;
    if (kind == "fade_missed" || kind == "banner_missed"){
        fault.kind = kind == "fade_missed" ? ReplayFaultKind::FADE_MISSED : ReplayFaultKind::BANNER_MISSED;
        fault.index = (size_t)json.get_integer_throw("index");
    }else if (kind == "moved_further" || kind == "hold_through_warp" || kind == "scene_ends_further"){
        fault.kind = kind == "moved_further" ? ReplayFaultKind::MOVED_FURTHER
            : kind == "hold_through_warp" ? ReplayFaultKind::HOLD_THROUGH_WARP : ReplayFaultKind::SCENE_ENDS_FURTHER;
        fault.step = json.get_string_throw("step");
        fault.direction = direction_of(name, json.get_string_throw("direction"));
        fault.tiles = (size_t)json.get_integer_default("tiles", 1);
        fault.fade = fade_of(name, json.get_string_default("fade", "unseen"));
        fault.far_pose_of = json.get_string_default("far_pose_of", "");
    }else if (kind == "moved_less"){
        fault.kind = ReplayFaultKind::MOVED_LESS;
        fault.step = json.get_string_throw("step");
        fault.index = (size_t)json.get_integer_throw("index");
        fault.tiles = (size_t)json.get_integer_throw("tiles");
    }else if (kind == "person_stands"){
        fault.kind = ReplayFaultKind::PERSON_STANDS;
        fault.name = json.get_string_throw("name");
        fault.tile = tile_of(name, json.get_array_throw("tile"));
    }else{
        bad(name, std::format("unknown fault kind \"{}\"", kind));
    }
    return fault;
}

const EnumStringMap<ReplayFaultKind>& replay_fault_kind_names(){
    static const EnumStringMap<ReplayFaultKind> MAP{
        {ReplayFaultKind::FADE_MISSED,        "fade_missed"},
        {ReplayFaultKind::BANNER_MISSED,      "banner_missed"},
        {ReplayFaultKind::MOVED_FURTHER,      "moved_further"},
        {ReplayFaultKind::HOLD_THROUGH_WARP,  "hold_through_warp"},
        {ReplayFaultKind::SCENE_ENDS_FURTHER, "scene_ends_further"},
        {ReplayFaultKind::PERSON_STANDS,      "person_stands"},
        {ReplayFaultKind::MOVED_LESS,         "moved_less"},
    };
    return MAP;
}

} // namespace


std::string replay_fault_kind_name(ReplayFaultKind kind){
    return replay_fault_kind_names().get_string(kind);
}

ConsoleFailure parse_console_failure(const std::string& name, const std::string& text){
    const JsonValue value = parse_json(text);
    const JsonObject& json = value.to_object_throw();
    ConsoleFailure out;
    out.name = name;
    out.checkpoint = json.get_string_throw("checkpoint");
    if (const JsonObject* start = json.get_object("start")){
        out.start = ReplayTile{start->get_string_throw("map"), tile_of(name, start->get_array_throw("tile"))};
        out.start_facing = direction_of(name, start->get_string_throw("facing"));
    }
    for (const auto& item : json.get_array_throw("faults")){
        out.faults.push_back(fault_of(name, item.to_object_throw()));
    }
    const JsonObject& failed = json.get_object_throw("failed");
    out.failed_step = failed.get_string_throw("step");
    out.failed_error = failed.get_string_throw("error");
    out.frame = json.get_string_throw("frame");
    if (const JsonArray* evidence = json.get_array("evidence")){
        for (const auto& line : *evidence){
            out.evidence.push_back(line.to_string_throw());
        }
    }
    return out;
}


} // namespace Pokemon
} // namespace PokemonAutomation
