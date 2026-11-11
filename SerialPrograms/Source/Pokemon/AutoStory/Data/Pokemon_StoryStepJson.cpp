/*  Story Step JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <array>
#include <format>
#include <initializer_list>
#include <limits>
#include <memory>
#include <set>
#include <span>
#include <utility>
#include <variant>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon_StoryStepJson.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  The wire names of the step kinds.
constexpr std::array<std::pair<StoryStepKind, std::string_view>, 15> KINDS{{
    {StoryStepKind::NEW_GAME, "new_game"},
    {StoryStepKind::OPTIONS, "options"},
    {StoryStepKind::ROUTE, "route"},
    {StoryStepKind::LEGS, "legs"},
    {StoryStepKind::STEP, "step"},
    {StoryStepKind::TALK, "talk"},
    {StoryStepKind::ANSWER, "answer"},
    {StoryStepKind::MASH, "scene"},
    {StoryStepKind::BATTLE, "battle"},
    {StoryStepKind::TRAIN, "train"},
    {StoryStepKind::HEAL, "heal"},
    {StoryStepKind::SHOP, "shop"},
    {StoryStepKind::SAVE, "save"},
    {StoryStepKind::READY, "ready"},
    {StoryStepKind::GAME, "game_action"},
}};

//  The wire names of the directions.
constexpr std::array<std::pair<Direction, std::string_view>, 4> DIRECTIONS{{
    {Direction::NORTH, "north"},
    {Direction::EAST, "east"},
    {Direction::SOUTH, "south"},
    {Direction::WEST, "west"},
}};


//  `value` as an object with no field outside `keys`.
const JsonObject& fields(const JsonValue& value, std::initializer_list<std::string_view> keys, std::string_view source){
    const JsonObject& object = value.to_object_throw(std::string(source));
    for (const auto& [key, unused] : object){
        bool known = false;
        for (std::string_view allowed : keys){
            known |= key == allowed;
        }
        if (!known){
            throw ParseException(std::format("{}: unsupported field {}", source, key));
        }
    }
    return object;
}

template <typename Enum, size_t N>
Enum read_enum(const std::array<std::pair<Enum, std::string_view>, N>& names, std::string_view name, std::string_view source){
    for (const auto& [value, label] : names){
        if (label == name){
            return value;
        }
    }
    throw ParseException(std::format("{}: unsupported value {}", source, name));
}

template <typename Enum, size_t N>
std::string enum_name(const std::array<std::pair<Enum, std::string_view>, N>& names, Enum value){
    for (const auto& [candidate, label] : names){
        if (candidate == value){
            return std::string(label);
        }
    }
    throw ParseException("Unsupported story enum value.");
}

int integer(const JsonValue& value, std::string_view source){
    const int64_t number = value.to_integer_throw(std::string(source));
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()){
        throw ParseException(std::format("{}: coordinate exceeds native int", source));
    }
    return (int)number;
}

size_t count(const JsonValue& value, std::string_view source){
    const int64_t number = value.to_integer_throw(std::string(source));
    if (number < 0 || (uint64_t)number > std::numeric_limits<size_t>::max()){
        throw ParseException(std::format("{}: count is out of range", source));
    }
    return (size_t)number;
}

JsonValue count_json(size_t value){
    if ((uint64_t)value > (uint64_t)std::numeric_limits<int64_t>::max()){
        throw ParseException("Story count exceeds the wire integer range.");
    }
    return (uint64_t)value;
}

JsonArray strings_json(std::span<const std::string> strings){
    JsonArray result;
    for (const std::string& value : strings){
        result.push_back(value);
    }
    return result;
}

std::vector<std::string> read_strings(const JsonValue& value, std::string_view source){
    std::vector<std::string> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        result.push_back(item.to_string_throw(std::string(source)));
    }
    return result;
}

JsonArray legs_json(std::span<const StoryLeg> legs){
    JsonArray result;
    for (const StoryLeg& leg : legs){
        JsonObject item;
        item["direction"] = enum_name(DIRECTIONS, leg.direction);
        item["tiles"] = leg.tiles;
        result.push_back(std::move(item));
    }
    return result;
}

std::vector<StoryLeg> read_legs(const JsonValue& value, std::string_view source){
    std::vector<StoryLeg> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        const JsonObject& row = fields(item, {"direction", "tiles"}, source);
        result.push_back({.direction = read_enum(DIRECTIONS, row.get_string_throw("direction"), source),
                          .tiles = integer(row.get_value_throw("tiles"), source)});
    }
    return result;
}

JsonArray items_json(std::span<const StoryItem> items){
    JsonArray result;
    for (const StoryItem& item : items){
        JsonObject row;
        row["item"] = item.item;
        row["quantity"] = count_json(item.quantity);
        result.push_back(std::move(row));
    }
    return result;
}

std::vector<StoryItem> read_items(const JsonValue& value, std::string_view source){
    std::vector<StoryItem> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        const JsonObject& row = fields(item, {"item", "quantity"}, source);
        result.push_back({.item = row.get_string_throw("item"), .quantity = count(row.get_value_throw("quantity"), source)});
    }
    return result;
}

JsonArray integer_rows_json(std::span<const std::vector<int64_t>> rows){
    JsonArray result;
    for (const std::vector<int64_t>& row : rows){
        JsonArray numbers;
        for (int64_t number : row){
            numbers.push_back(number);
        }
        result.push_back(std::move(numbers));
    }
    return result;
}

std::vector<std::vector<int64_t>> read_integer_rows(const JsonValue& value, std::string_view source){
    std::vector<std::vector<int64_t>> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        std::vector<int64_t> row;
        for (const JsonValue& number : item.to_array_throw(std::string(source))){
            row.push_back(number.to_integer_throw(std::string(source)));
        }
        result.push_back(std::move(row));
    }
    return result;
}

JsonArray timed_legs_json(std::span<const StoryTimedLeg> legs){
    JsonArray result;
    for (const StoryTimedLeg& leg : legs){
        JsonArray pair;
        pair.push_back(leg.turn);
        pair.push_back(leg.ms);
        result.push_back(std::move(pair));
    }
    return result;
}

std::vector<StoryTimedLeg> read_timed_legs(const JsonValue& value, std::string_view source){
    std::vector<StoryTimedLeg> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        const JsonArray& pair = item.to_array_throw(std::string(source));
        if (pair.size() != 2){
            throw ParseException(std::format("{}: a timed leg is [turn degrees, ms]", source));
        }
        result.push_back({.turn = integer(pair[0], source), .ms = integer(pair[1], source)});
    }
    return result;
}

JsonArray tiles_json(std::span<const TilePos> tiles){
    JsonArray result;
    for (const TilePos& tile : tiles){
        JsonArray pair;
        pair.push_back(tile.x);
        pair.push_back(tile.y);
        result.push_back(std::move(pair));
    }
    return result;
}

std::vector<TilePos> read_tiles(const JsonValue& value, std::string_view source){
    std::vector<TilePos> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        const JsonArray& pair = item.to_array_throw(std::string(source));
        if (pair.size() != 2){
            throw ParseException(std::format("{}: a tile needs two coordinates", source));
        }
        result.push_back({integer(pair[0], source), integer(pair[1], source)});
    }
    return result;
}

//  One arg's value as JSON.
JsonValue arg_json(const StoryArgValue& value){
    switch (story_arg_type(value)){
    case StoryArgType::BOOLEAN:
        return std::get<bool>(value);
    case StoryArgType::INTEGER:
        return std::get<int64_t>(value);
    case StoryArgType::WORD:
        return std::get<std::string>(value);
    case StoryArgType::DIRECTION:
        return enum_name(DIRECTIONS, std::get<Direction>(value));
    case StoryArgType::GRID_POSE:
        return grid_pose_json(std::get<GridPose>(value));
    case StoryArgType::LEGS:
        return legs_json(std::get<std::vector<StoryLeg>>(value));
    case StoryArgType::WORDS:
        return strings_json(std::get<std::vector<std::string>>(value));
    case StoryArgType::ITEMS:
        return items_json(std::get<std::vector<StoryItem>>(value));
    case StoryArgType::INTEGER_ROWS:
        return integer_rows_json(std::get<std::vector<std::vector<int64_t>>>(value));
    case StoryArgType::TIMED_LEGS:
        return timed_legs_json(std::get<std::vector<StoryTimedLeg>>(value));
    }
    throw ParseException("Unsupported story arg type.");
}

//  One arg's value read as `type`.
StoryArgValue read_arg(const JsonValue& value, StoryArgType type, std::string_view source){
    switch (type){
    case StoryArgType::BOOLEAN:
        return value.to_boolean_throw(std::string(source));
    case StoryArgType::INTEGER:
        return value.to_integer_throw(std::string(source));
    case StoryArgType::WORD:
        return value.to_string_throw(std::string(source));
    case StoryArgType::DIRECTION:
        return read_enum(DIRECTIONS, value.to_string_throw(std::string(source)), source);
    case StoryArgType::GRID_POSE:
        return read_grid_pose(value, source);
    case StoryArgType::LEGS:
        return read_legs(value, source);
    case StoryArgType::WORDS:
        return read_strings(value, source);
    case StoryArgType::ITEMS:
        return read_items(value, source);
    case StoryArgType::INTEGER_ROWS:
        return read_integer_rows(value, source);
    case StoryArgType::TIMED_LEGS:
        return read_timed_legs(value, source);
    }
    throw ParseException(std::format("{}: unsupported story arg type", source));
}

//  The step's action among `actions`: the named one, or the kind's default.
//  Null for a kind with no action that names none.
const StoryActionSpec* step_action(
    const StoryStep& step, std::span<const StoryActionSpec> actions, std::string_view source
){
    if (step.kind == StoryStepKind::GAME && step.action.empty()){
        throw ParseException(std::format("{}: game_action step {} names no action", source, step.name));
    }
    const StoryActionSpec* spec = find_story_action(actions, step.kind, step.action);
    if (spec == nullptr && !step.action.empty()){
        throw ParseException(std::format("{}: step {}: unknown action {}", source, step.name, step.action));
    }
    return spec;
}

//  The step's args, read against its action's spec, with the fallbacks
//  filled in.
StoryActionArgs read_args(
    const JsonValue* value, const StoryActionSpec* spec, const StoryStep& step, std::string_view source
){
    const StoryActionSpec none{.kind = step.kind, .name = "", .args = {}};
    const StoryActionSpec& action = spec ? *spec : none;
    StoryActionArgs result;
    if (value != nullptr){
        for (const auto& [key, item] : value->to_object_throw(std::string(source))){
            const auto arg = std::find_if(action.args.begin(), action.args.end(), [&](const StoryArgSpec& candidate){
                return candidate.key == key;
            });
            if (arg == action.args.end()){
                throw ParseException(std::format("{}: step {}: unknown argument \"{}\"", source, step.name, key));
            }
            result[key] = read_arg(item, arg->type, source);
        }
    }
    const std::vector<std::string> errors = story_args_errors(action, result);
    if (!errors.empty()){
        throw ParseException(std::format("{}: step {}: {}", source, step.name, errors.front()));
    }
    return with_fallbacks(action, std::move(result));
}


JsonValue postconditions_json(const StoryPostconditions& postconditions){
    JsonObject result;
    if (postconditions.position){
        result["position"] = grid_pose_json(*postconditions.position);
    }
    if (!postconditions.items.empty()){
        result["items"] = items_json(postconditions.items);
    }
    if (!postconditions.flags.empty()){
        result["flags"] = strings_json(postconditions.flags);
    }
    return result;
}

StoryPostconditions read_postconditions(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"position", "items", "flags"}, source);
    StoryPostconditions result;
    if (const JsonValue* field = root.get_value("position")){
        result.position = read_grid_pose(*field, source);
    }
    if (const JsonValue* field = root.get_value("items")){
        result.items = read_items(*field, source);
    }
    if (const JsonValue* field = root.get_value("flags")){
        result.flags = read_strings(*field, source);
    }
    return result;
}


JsonValue verification_json(const StoryVerification& verified){
    JsonObject result;
    result["steps_hash"] = verified.steps_hash;
    result["build"] = verified.build;
    result["date"] = verified.date;
    result["runs"] = verified.runs;
    return result;
}

StoryVerification read_verification(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"steps_hash", "build", "date", "runs"}, source);
    StoryVerification result;
    result.steps_hash = root.get_string_throw("steps_hash");
    if (const JsonValue* field = root.get_value("build")){
        result.build = field->to_string_throw();
    }
    if (const JsonValue* field = root.get_value("date")){
        result.date = field->to_string_throw();
    }
    if (const JsonValue* field = root.get_value("runs")){
        result.runs = field->to_string_throw();
    }
    return result;
}

//  The "metadata" object: the phase, the verification, the source and the
//  postconditions.
JsonValue metadata_json(const StoryCheckpoint& checkpoint){
    JsonObject result;
    result["phase"] = story_phase_name(checkpoint.metadata.phase);
    if (checkpoint.metadata.verified){
        result["verified"] = verification_json(*checkpoint.metadata.verified);
    }
    if (!checkpoint.metadata.source.empty()){
        result["source"] = checkpoint.metadata.source;
    }
    if (checkpoint.postconditions != StoryPostconditions{}){
        result["postconditions"] = postconditions_json(checkpoint.postconditions);
    }
    return result;
}

//  Reads "metadata" into `checkpoint`. A missing phase is testing.
void read_metadata(const JsonValue& value, std::string_view source, StoryCheckpoint& checkpoint){
    const JsonObject& root = fields(value, {"phase", "verified", "source", "postconditions"}, source);
    if (const JsonValue* field = root.get_value("phase")){
        const std::string name = field->to_string_throw();
        const std::optional<StoryPhase> phase = read_story_phase(name);
        if (!phase){
            throw ParseException(std::format("{}: unknown phase {}", source, name));
        }
        checkpoint.metadata.phase = *phase;
    }
    if (const JsonValue* field = root.get_value("verified")){
        checkpoint.metadata.verified = read_verification(*field, source);
    }
    if (const JsonValue* field = root.get_value("source")){
        checkpoint.metadata.source = field->to_string_throw();
    }
    if (const JsonValue* field = root.get_value("postconditions")){
        checkpoint.postconditions = read_postconditions(*field, source);
    }
}


} // namespace


JsonValue grid_pose_json(const GridPose& pose){
    JsonObject result;
    result["map"] = pose.map;
    result["tiles"] = tiles_json(pose.tiles);
    if (pose.facing){
        result["facing"] = enum_name(DIRECTIONS, *pose.facing);
    }
    return result;
}

GridPose read_grid_pose(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"map", "tiles", "facing"}, source);
    GridPose result;
    result.map = root.get_string_throw("map");
    //  None: any tile of the map, or a game with no tiles (a zone).
    if (const JsonValue* field = root.get_value("tiles")){
        result.tiles = read_tiles(*field, source);
    }
    if (const JsonValue* field = root.get_value("facing")){
        result.facing = read_enum(DIRECTIONS, field->to_string_throw(), source);
    }
    return result;
}

JsonValue declared_pose_json(const DeclaredPose& pose){
    if (const TileDeclared* tile = std::get_if<TileDeclared>(&pose)){
        return grid_pose_json(GridPose{.map = tile->map, .tiles = tile->tiles, .facing = tile->facing});
    }
    const ZoneDeclared& zone = std::get<ZoneDeclared>(pose);
    JsonObject result;
    result["place"] = zone.place;
    if (zone.spot){
        result["spot"] = *zone.spot;
    }
    if (zone.heading){
        result["heading"] = *zone.heading;
    }
    return result;
}

DeclaredPose read_declared_pose(const JsonValue& value, std::string_view source){
    const JsonObject& root = value.to_object_throw(std::string(source));
    const bool tile = root.get_value("map") != nullptr;
    const bool zone = root.get_value("place") != nullptr;
    if (tile == zone){
        throw ParseException(std::format("{}: a declared pose has a \"map\" (tiles) or a \"place\" (a zone), not {}", source, tile ? "both" : "neither"));
    }
    if (tile){
        GridPose grid = read_grid_pose(value, source);
        return TileDeclared{.map = std::move(grid.map), .tiles = std::move(grid.tiles), .facing = grid.facing};
    }
    fields(value, {"place", "spot", "heading"}, source);
    ZoneDeclared result;
    result.place = root.get_string_throw("place");
    if (const JsonValue* field = root.get_value("spot")){
        result.spot = field->to_string_throw(std::string(source));
    }
    if (const JsonValue* field = root.get_value("heading")){
        result.heading = integer(*field, source);
    }
    return result;
}

JsonValue story_step_json(const StoryStep& step){
    JsonObject result;
    result["id"] = step.name;
    result["kind"] = enum_name(KINDS, step.kind);
    if (!step.why.empty()){
        result["why"] = step.why;
    }
    if (!step.action.empty()){
        result["action"] = step.action;
    }
    if (!step.args.empty()){
        JsonObject args;
        for (const auto& [key, value] : step.args){
            args[key] = arg_json(value);
        }
        result["args"] = std::move(args);
    }
    if (!step.skip_if.empty()){
        result["skip_if"] = strings_json(step.skip_if);
    }
    if (step.pose_after){
        result["declared_after"] = declared_pose_json(*step.pose_after);
    }
    return result;
}

StoryStep read_story_step(const JsonValue& value, std::string_view source, std::span<const StoryActionSpec> actions){
    const JsonObject& root = fields(value, {"id", "kind", "why", "action", "args", "skip_if", "declared_after"}, source);
    StoryStep result;
    result.name = root.get_string_throw("id");
    if (result.name.empty()){
        throw ParseException("Empty story step id.");
    }
    result.kind = read_enum(KINDS, root.get_string_throw("kind"), source);
    if (const JsonValue* field = root.get_value("why")){
        result.why = field->to_string_throw();
    }
    if (const JsonValue* field = root.get_value("action")){
        result.action = field->to_string_throw();
    }
    result.args = read_args(root.get_value("args"), step_action(result, actions, source), result, source);
    if (const JsonValue* field = root.get_value("skip_if")){
        result.skip_if = read_strings(*field, source);
    }
    if (const JsonValue* field = root.get_value("declared_after")){
        result.pose_after = read_declared_pose(*field, source);
    }
    return result;
}

JsonValue story_checkpoint_json(const StoryCheckpoint& checkpoint){
    JsonObject result;
    result["id"] = checkpoint.id;
    result["title"] = checkpoint.title;
    if (checkpoint.start){
        result["start"] = declared_pose_json(*checkpoint.start);
    }
    JsonArray events;
    for (const StoryStep& step : checkpoint.steps){
        events.push_back(story_step_json(step));
    }
    result["events"] = std::move(events);
    if (checkpoint.postconditions != StoryPostconditions{} || checkpoint.metadata != StoryCheckpointMetadata{}){
        result["metadata"] = metadata_json(checkpoint);
    }
    return result;
}

StoryCheckpoint read_story_checkpoint(const JsonValue& value, std::string_view source, std::span<const StoryActionSpec> actions){
    const JsonObject& root = fields(value, {"id", "title", "start", "events", "metadata"}, source);
    StoryCheckpoint result;
    result.id = root.get_string_throw("id");
    if (result.id.empty()){
        throw ParseException("Empty story checkpoint id.");
    }
    result.title = root.get_string_throw("title");
    if (const JsonValue* start = root.get_value("start")){
        result.start = read_declared_pose(*start, source);
    }
    std::set<std::string> names;
    for (const JsonValue& event : root.get_array_throw("events")){
        StoryStep step = read_story_step(event, source, actions);
        if (step.name.empty() || !names.insert(step.name).second){
            throw ParseException("Empty or duplicate story event id.");
        }
        result.steps.push_back(std::move(step));
    }
    if (const JsonValue* metadata = root.get_value("metadata")){
        read_metadata(*metadata, source, result);
    }
    return result;
}



} // namespace Pokemon
} // namespace PokemonAutomation
