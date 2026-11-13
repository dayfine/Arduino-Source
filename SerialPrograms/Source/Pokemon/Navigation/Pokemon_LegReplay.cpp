/*  Leg Replay
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <stdexcept>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon_LegReplay.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

[[noreturn]] void bad(const std::string& name, const std::string& what){
    throw std::runtime_error("console failure " + name + ": " + what);
}

Direction direction_of(const std::string& name, const std::string& text){
    for (const Direction direction : ALL_DIRECTIONS){
        if (direction_name(direction) == text){
            return direction;
        }
    }
    bad(name, "unknown direction \"" + text + "\"");
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
    bad(name, "unknown fade \"" + text + "\"");
}

ReplayFault fault_of(const std::string& name, const JsonObject& json){
    const std::string kind = json.get_string_throw("kind");
    ReplayFault fault;
    if (kind == "fade_missed" || kind == "banner_missed"){
        fault.kind = kind == "fade_missed" ? ReplayFaultKind::FADE_MISSED : ReplayFaultKind::BANNER_MISSED;
        fault.index = (size_t)json.get_integer_throw("index");
    }else if (kind == "moved_further" || kind == "hold_through_warp"){
        fault.kind = kind == "moved_further" ? ReplayFaultKind::MOVED_FURTHER : ReplayFaultKind::HOLD_THROUGH_WARP;
        fault.step = json.get_string_throw("step");
        fault.direction = direction_of(name, json.get_string_throw("direction"));
        fault.tiles = (size_t)json.get_integer_default("tiles", 1);
        fault.fade = fade_of(name, json.get_string_default("fade", "unseen"));
        fault.far_pose_of = json.get_string_default("far_pose_of", "");
    }else if (kind == "person_stands"){
        fault.kind = ReplayFaultKind::PERSON_STANDS;
        fault.name = json.get_string_throw("name");
        fault.tile = tile_of(name, json.get_array_throw("tile"));
    }else{
        bad(name, "unknown fault kind \"" + kind + "\"");
    }
    return fault;
}

} // namespace


std::string replay_fault_kind_name(ReplayFaultKind kind){
    switch (kind){
    case ReplayFaultKind::FADE_MISSED:
        return "fade_missed";
    case ReplayFaultKind::BANNER_MISSED:
        return "banner_missed";
    case ReplayFaultKind::MOVED_FURTHER:
        return "moved_further";
    case ReplayFaultKind::HOLD_THROUGH_WARP:
        return "hold_through_warp";
    case ReplayFaultKind::PERSON_STANDS:
        return "person_stands";
    }
    return "";
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
