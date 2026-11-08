/*  Story Step JSON
 *  From: https://github.com/PokemonAutomation/
 */
#include <array>
#include <format>
#include <initializer_list>
#include <limits>
#include <set>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon_StoryStepJson.h"
namespace PokemonAutomation{
namespace Pokemon{
namespace{
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
constexpr std::array<std::pair<PoseCertainty, std::string_view>, 5> CERTAINTIES{{
    {PoseCertainty::TILE, "tile"},
    {PoseCertainty::COLUMN, "column"},
    {PoseCertainty::ROW, "row"},
    {PoseCertainty::SET, "set"},
    {PoseCertainty::UNKNOWN, "unknown"},
}};
constexpr std::array<std::pair<Direction, std::string_view>, 4> DIRECTIONS{{
    {Direction::NORTH, "north"},
    {Direction::EAST, "east"},
    {Direction::SOUTH, "south"},
    {Direction::WEST, "west"},
}};

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
Enum read_enum(const std::array<std::pair<Enum, std::string_view>, N>& names, const std::string& name, std::string_view source){
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

JsonArray strings_json(const std::vector<std::string>& strings){
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

JsonArray legs_json(const std::vector<StoryLeg>& legs){
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

JsonArray items_json(const std::vector<StoryItem>& items){
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
StoryStep read_step_options(StoryStep result, const JsonObject& root, std::string_view source){
    if (const JsonValue* field = root.get_value("fight_wild")){
        result.fight_wild = field->to_boolean_throw();
    }
    if (const JsonValue* field = root.get_value("yes")){
        result.yes = field->to_boolean_throw();
    }
    if (const JsonValue* field = root.get_value("may_lose")){
        result.may_lose = field->to_boolean_throw();
    }
    if (const JsonValue* field = root.get_value("battles")){
        result.battles = count(*field, source);
    }
    return result;
}

} // namespace

JsonValue story_pose_json(const Pose& pose){
    JsonObject result;
    result["map"] = pose.map;
    result["certainty"] = enum_name(CERTAINTIES, pose.certainty);
    JsonObject where;
    where["kind"] = "unknown";
    if (pose.where){
        const auto* tiles = dynamic_cast<const TilePose*>(pose.where.get());
        if (!tiles){
            throw ParseException("Unsupported story Position subtype.");
        }
        where["kind"] = "tiles";
        JsonArray positions;
        for (const TilePos& tile : tiles->tiles){
            JsonArray pair;
            pair.push_back(tile.x); pair.push_back(tile.y);
            positions.push_back(std::move(pair));
        }
        where["tiles"] = std::move(positions);
        if (tiles->facing){
            where["facing"] = enum_name(DIRECTIONS, *tiles->facing);
        }
    }
    result["where"] = std::move(where);
    return result;
}

Pose read_story_pose(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"map", "certainty", "where"}, source);
    Pose result;
    result.map = root.get_string_throw("map");
    result.certainty = read_enum(CERTAINTIES, root.get_string_throw("certainty"), source);
    const JsonObject& where = fields(root.get_value_throw("where"), {"kind", "tiles", "facing"}, source);
    const std::string& kind = where.get_string_throw("kind");
    if (kind == "unknown"){
        if (where.size() != 1){
            throw ParseException("Unknown story position has extra fields.");
        }
        return result;
    }
    if (kind != "tiles"){
        throw ParseException("Unsupported story position kind.");
    }
    std::vector<TilePos> tiles;
    for (const JsonValue& item : where.get_array_throw("tiles")){
        const JsonArray& pair = item.to_array_throw(std::string(source));
        if (pair.size() != 2){
            throw ParseException("Story tile needs two coordinates.");
        }
        tiles.push_back({integer(pair[0], source), integer(pair[1], source)});
    }
    std::optional<Direction> facing;
    if (const JsonValue* field = where.get_value("facing")){
        facing = read_enum(DIRECTIONS, field->to_string_throw(), source);
    }
    result.where = std::make_shared<TilePose>(std::move(tiles), facing);
    return result;
}

JsonValue story_step_json(const StoryStep& step){
    JsonObject result;
    result["id"] = step.name;
    result["kind"] = enum_name(KINDS, step.kind);
    if (!step.why.empty()){
        result["why"] = step.why;
    }
    if (step.target){
        result["target"] = story_pose_json(*step.target);
    }
    if (step.direction != Direction::NORTH){
        result["direction"] = enum_name(DIRECTIONS, step.direction);
    }
    if (!step.legs.empty()){
        result["legs"] = legs_json(step.legs);
    }
    if (step.fight_wild){
        result["fight_wild"] = true;
    }
    if (!step.yes){
        result["yes"] = false;
    }
    if (step.may_lose){
        result["may_lose"] = true;
    }
    if (step.battles){
        result["battles"] = count_json(step.battles);
    }
    if (!step.items.empty()){
        result["items"] = items_json(step.items);
    }
    if (!step.variant.empty()){
        result["variant"] = step.variant;
    }
    if (!step.skip_if.empty()){
        result["skip_if"] = strings_json(step.skip_if);
    }
    if (step.pose_after){
        result["declared_after"] = story_pose_json(*step.pose_after);
    }
    return result;
}

StoryStep read_story_step(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"id", "kind", "why", "target", "direction", "legs", "fight_wild", "yes",
        "may_lose", "battles", "items", "variant", "skip_if", "declared_after"}, source);
    StoryStep result;
    result.name = root.get_string_throw("id");
    if (result.name.empty()){
        throw ParseException("Empty story step id.");
    }
    result.kind = read_enum(KINDS, root.get_string_throw("kind"), source);
    if (const JsonValue* field = root.get_value("why")){
        result.why = field->to_string_throw();
    }
    if (const JsonValue* field = root.get_value("target")){
        result.target = read_story_pose(*field, source);
    }
    if (const JsonValue* field = root.get_value("direction")){
        result.direction = read_enum(DIRECTIONS, field->to_string_throw(), source);
    }
    if (const JsonValue* field = root.get_value("legs")){
        result.legs = read_legs(*field, source);
    }
    result = read_step_options(std::move(result), root, source);
    if (const JsonValue* field = root.get_value("items")){
        result.items = read_items(*field, source);
    }
    if (const JsonValue* field = root.get_value("variant")){
        result.variant = field->to_string_throw();
    }
    if (const JsonValue* field = root.get_value("skip_if")){
        result.skip_if = read_strings(*field, source);
    }
    if (const JsonValue* field = root.get_value("declared_after")){
        result.pose_after = read_story_pose(*field, source);
    }
    return result;
}

JsonValue story_checkpoint_json(const StoryCheckpoint& checkpoint){
    JsonObject result;
    result["id"] = checkpoint.id;
    result["title"] = checkpoint.title;
    if (checkpoint.start){
        result["start"] = story_pose_json(*checkpoint.start);
    }
    JsonArray events;
    for (const StoryStep& step : checkpoint.steps){
        events.push_back(story_step_json(step));
    }
    result["events"] = std::move(events);
    return result;
}

StoryCheckpoint read_story_checkpoint(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"id", "title", "start", "events"}, source);
    StoryCheckpoint result;
    result.id = root.get_string_throw("id");
    if (result.id.empty()){
        throw ParseException("Empty story checkpoint id.");
    }
    result.title = root.get_string_throw("title");
    if (const JsonValue* start = root.get_value("start")){
        result.start = read_story_pose(*start, source);
    }
    std::set<std::string> names;
    for (const JsonValue& event : root.get_array_throw("events")){
        StoryStep step = read_story_step(event, source);
        if (step.name.empty() || !names.insert(step.name).second){
            throw ParseException("Empty or duplicate story event id.");
        }
        result.steps.push_back(std::move(step));
    }
    return result;
}
} // namespace Pokemon
} // namespace PokemonAutomation
