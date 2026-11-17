/*  Leg Trace
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <charconv>
#include <utility>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon_LegTrace.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string_view leg_end_kind_name(LegEndKind kind){
    switch (kind){
    case LegEndKind::OPEN:
        return "open";
    case LegEndKind::WALL:
        return "wall";
    case LegEndKind::TILE:
        return "tile";
    }
    return "?";
}

std::string_view leg_end_known_name(LegEndKnown known){
    switch (known){
    case LegEndKnown::WALL:
        return "wall";
    case LegEndKnown::FADE:
        return "fade";
    case LegEndKnown::DIALOGUE:
        return "dialogue";
    case LegEndKnown::COUNTED:
        return "counted";
    case LegEndKnown::DEAD_RECKONED:
        return "dead_reckoned";
    case LegEndKnown::SEEN:
        return "seen";
    case LegEndKnown::UNKNOWN:
        return "unknown";
    }
    return "?";
}

std::string_view leg_interrupt_name(LegInterrupt interrupt){
    switch (interrupt){
    case LegInterrupt::WILD_BATTLE:
        return "wild_battle";
    case LegInterrupt::TRAINER:
        return "trainer";
    case LegInterrupt::SCENE:
        return "scene";
    case LegInterrupt::DIALOGUE:
        return "dialogue";
    case LegInterrupt::BLOCKED:
        return "blocked";
    }
    return "?";
}


namespace{

//  [["Route 202", 23, 25], ...]
JsonArray tiles_json(const std::vector<RouteTile>& tiles){
    JsonArray out;
    for (const RouteTile& tile : tiles){
        JsonArray entry;
        entry.push_back(tile.map);
        entry.push_back((int64_t)tile.pos.x);
        entry.push_back((int64_t)tile.pos.y);
        out.push_back(std::move(entry));
    }
    return out;
}

} // namespace

std::string leg_trace_json(const LegRecord& record){
    JsonObject out;
    out["checkpoint"] = record.checkpoint;
    out["event"] = record.event;
    out["leg"] = (int64_t)record.leg;
    out["dir"] = direction_name(record.plan.direction);
    out["tiles"] = (int64_t)record.plan.tiles;
    out["end_kind"] = std::string(leg_end_kind_name(record.plan.end_kind));
    out["planned_end"] = tiles_json(record.plan.ends);
    if (record.moved){
        out["moved"] = (int64_t)*record.moved;
    }
    out["chunks"] = (int64_t)record.chunks;
    out["ms"] = record.walked_ms;
    out["end"] = tiles_json(record.end);
    out["known"] = std::string(leg_end_known_name(record.known));
    JsonArray interrupts;
    for (LegInterrupt interrupt : record.interrupts){
        interrupts.push_back(std::string(leg_interrupt_name(interrupt)));
    }
    out["interrupts"] = std::move(interrupts);
    if (!record.note.empty()){
        out["note"] = record.note;
    }
    return out.dump(-1);
}

std::string leg_trace_line(const LegRecord& record){
    return std::string(LEG_TRACE_TAG) + leg_trace_json(record);
}


namespace{

std::optional<int> parse_int(std::string_view text){
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size()){
        return std::nullopt;
    }
    return value;
}

//  "Route 202 (23, 25)".
std::optional<RouteTile> parse_route_tile(std::string_view text){
    const size_t open = text.rfind(" (");
    const size_t comma = text.rfind(", ");
    if (open == std::string_view::npos || comma == std::string_view::npos || comma < open || !text.ends_with(")")){
        return std::nullopt;
    }
    const std::optional<int> x = parse_int(text.substr(open + 2, comma - open - 2));
    const std::optional<int> y = parse_int(text.substr(comma + 2, text.size() - comma - 3));
    if (open == 0 || !x || !y){
        return std::nullopt;
    }
    return RouteTile{std::string(text.substr(0, open)), TilePos{*x, *y}};
}

std::optional<Direction> parse_direction(std::string_view text){
    for (Direction direction : ALL_DIRECTIONS){
        if (text == direction_name(direction)){
            return direction;
        }
    }
    return std::nullopt;
}

} // namespace

std::vector<RouteTile> parse_route_tiles(std::string_view text){
    std::vector<RouteTile> out;
    while (!text.empty()){
        //  Each tile ends at its ")"; ", " separates them.
        const size_t close = text.find(')');
        if (close == std::string_view::npos){
            return {};
        }
        const std::optional<RouteTile> tile = parse_route_tile(text.substr(0, close + 1));
        if (!tile){
            return {};
        }
        out.push_back(*tile);
        text.remove_prefix(close + 1);
        if (text.starts_with(", ")){
            text.remove_prefix(2);
        }else if (!text.empty()){
            return {};
        }
    }
    return out;
}

std::optional<LegPlan> parse_route_leg_line(std::string_view text){
    //  "Leg <n>/<m>: <direction> <tiles> (<end>) to <tiles>"
    if (!text.starts_with("Leg ")){
        return std::nullopt;
    }
    const size_t colon = text.find(": ");
    if (colon == std::string_view::npos){
        return std::nullopt;
    }
    text.remove_prefix(colon + 2);
    const size_t space = text.find(' ');
    const size_t open = text.find(" (");
    const size_t close = text.find(") to ");
    if (space == std::string_view::npos || open == std::string_view::npos || close == std::string_view::npos
        || !(space <= open && open < close)){
        return std::nullopt;
    }
    const std::optional<Direction> direction = parse_direction(text.substr(0, space));
    const std::optional<int> tiles = parse_int(text.substr(space + 1, open - space - 1));
    const std::string_view end = text.substr(open + 2, close - open - 2);
    if (!direction || !tiles || *tiles < 0){
        return std::nullopt;
    }
    LegPlan plan{.direction = *direction, .tiles = (size_t)*tiles};
    if (end == "to wall"){
        plan.end_kind = LegEndKind::WALL;
    }else if (end == "warp"){
        plan.end_kind = LegEndKind::TILE;
    }else if (end == "open"){
        plan.end_kind = LegEndKind::OPEN;
    }else{
        return std::nullopt;
    }
    plan.ends = parse_route_tiles(text.substr(close + 5));
    return plan;
}


void LegTracer::begin_event(const std::string& checkpoint, const std::string& event){
    end_event();
    m_checkpoint = checkpoint;
    m_event = event;
    m_legs = 0;
}

void LegTracer::end_event(){
    if (m_leg){
        end_leg({}, LegEndKnown::UNKNOWN);
    }
}

void LegTracer::begin_leg(LegPlan plan){
    end_event();
    m_leg = LegRecord{.checkpoint = m_checkpoint, .event = m_event, .leg = ++m_legs, .plan = std::move(plan)};
}

void LegTracer::walked(const LegWalk& walk){
    if (!m_leg){
        return;
    }
    m_leg->chunks += walk.chunks;
    m_leg->walked_ms += walk.walked_ms;
    for (size_t i = 0; i < walk.wild_battles; i++){
        m_leg->interrupts.push_back(LegInterrupt::WILD_BATTLE);
    }
    if (walk.tapped){
        m_leg->moved = m_leg->moved.value_or(0) + *walk.tapped;
    }
}

void LegTracer::interrupted(LegInterrupt interrupt){
    if (m_leg){
        m_leg->interrupts.push_back(interrupt);
    }
}

void LegTracer::end_leg(std::vector<RouteTile> end, LegEndKnown known, std::string note){
    if (!m_leg){
        return;
    }
    LegRecord record = std::move(*m_leg);
    m_leg.reset();
    record.end = std::move(end);
    record.known = known;
    record.note = std::move(note);
    m_emit(leg_trace_line(record));
}



} // namespace Pokemon
} // namespace PokemonAutomation
