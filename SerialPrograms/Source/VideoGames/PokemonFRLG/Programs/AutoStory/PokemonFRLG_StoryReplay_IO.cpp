/*  Offline FRLG Story Replay Input
 *  From: https://github.com/PokemonAutomation/
 */
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <set>
#include "Common/Cpp/Cryptography/SHA256.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_StoryReplay_IO.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace StoryReplayInternal{
const JsonObject& fields(const JsonValue& value, std::initializer_list<std::string_view> names){
    const JsonObject& root = value.to_object_throw("story replay");
    if (root.size() != names.size()){
        throw ParseException("story replay: missing/extra object fields");
    }
    for (std::string_view name : names){
        root.get_value_throw(std::string(name));
    }
    return root;
}
std::string digest(std::string_view text){
    SHA256 hash;
    hash.push(text.data(), text.size());
    hash.finish();
    return hash.get_hash_hex();
}
Direction direction(std::string_view value){
    for (Direction candidate : {Direction::NORTH, Direction::EAST, Direction::SOUTH, Direction::WEST}){
        if (value == direction_name(candidate)){
            return candidate;
        }
    }
    throw ParseException("story replay: unknown direction");
}
const MapData& map_named(const std::vector<MapData>& maps, std::string_view name){
    for (const MapData& map : maps){
        if (map.name == name){
            return map;
        }
    }
    throw ParseException(std::format("story replay: unbound map {}", name));
}
namespace{
int coordinate(const JsonValue& value){
    const int64_t number = value.to_integer_throw();
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()){
        throw ParseException("story replay: coordinate outside int32");
    }
    return (int)number;
}
void check_pose(const OverworldPose& pose, const std::vector<MapData>& maps){
    const MapData& map = map_named(maps, pose.position.map);
    const TilePos tile = pose.position.point;
    if (!in_bounds(map, tile) || is_wall(map, tile) || object_at(map, tile) != nullptr){
        throw ParseException("story replay: pose is not a stable unoccupied tile");
    }
    const uint8_t level = elevation_at(map, tile);
    if (level != ELEVATION_ANY && level != ELEVATION_IGNORED && level != pose.elevation){
        throw ParseException("story replay: pose elevation disagrees with map");
    }
}
void expect(const JsonObject& root, std::string_view key, std::string_view value){
    if (root.get_string_throw(std::string(key)) != value){
        throw ParseException(std::format("story replay: unsupported {}", key));
    }
}
void check_policy(const JsonValue& value){
    const JsonObject& policy = fields(value, {"basis", "entry", "attempts", "failure", "save"});
    expect(policy, "basis", "synthetic-offline-policy");
    expect(policy, "entry", "scripted-observation");
    expect(policy, "failure", "stop");
    expect(policy, "save", "never");
    if (policy.get_integer_throw("attempts") != 1){
        throw ParseException("story replay: only one stop/no-save attempt is supported");
    }
}
std::vector<MapData> read_maps(const JsonObject& pins){
    if (pins.empty()){
        throw ParseException("story replay: map pins required");
    }
    std::vector<MapData> maps;
    for (const auto& [name, hash] : pins){
        const auto& files = overworld_map_json();
        const auto found = std::find_if(files.begin(), files.end(), [&](const auto& file){ return file.first == name; });
        if (found == files.end() || digest(found->second) != hash.to_string_throw()){
            throw ParseException(std::format("story replay: embedded map hash mismatch: {}", name));
        }
        maps.emplace_back(load_map(name));
    }
    return maps;
}
void receipts(const JsonObject& actual, const JsonObject* allowed){
    for (const auto& [name, count] : actual){
        if (name.empty() || count.to_integer_throw() <= 0 || (allowed && !allowed->get_value(name))){
            throw ParseException("story replay: unsupported receipt item/quantity");
        }
    }
}
void check_capability(const StoryStep& event, const JsonValue& value){
    const JsonObject& spec = fields(value, {"capability", "parameters"});
    const std::string capability = spec.get_string_throw("capability");
    if (event.kind == StoryStepKind::MASH){
        expect(spec, "capability", "scripted-scene/1");
        const JsonObject& params = fields(spec.get_value_throw("parameters"), {"require_received"});
        receipts(params.get_object_throw("require_received"), nullptr);
        if (!event.pose_after){
            throw ParseException("story replay: scene needs declared arrival");
        }
        return;
    }
    fields(spec.get_value_throw("parameters"), {});
    if ((event.kind != StoryStepKind::ROUTE || capability != "frlg-runtime-route/1")
        && (event.kind != StoryStepKind::STEP || capability != "frlg-runtime-step/1")){
        throw ParseException("story replay: unsupported kind/capability");
    }
    if (event.kind == StoryStepKind::ROUTE && !event.args.contains("target")){
        throw ParseException("story replay: route target missing");
    }
}
void check_spatial_options(const JsonObject& capture){
    for (const JsonValue& item : capture.get_object_throw("checkpoint").get_array_throw("events")){
        const JsonObject& step = item.to_object_throw();
        std::set<std::string> allowed{"id", "kind", "why"};
        const std::string kind = step.get_string_throw("kind");
        if (kind == "route"){
            allowed.insert("target");
        }else if (kind == "step"){
            allowed.insert("direction");
        }else if (kind == "scene"){
            allowed.insert("declared_after");
        }else{
            throw ParseException("story replay: unsupported event kind");
        }
        for (const auto& [key, value] : step){
            if (!allowed.contains(key)){
                throw ParseException("story replay: unsupported event option " + key);
            }
        }
    }
}
void revision(std::string_view value){
    if (value.size() != 40 || !std::all_of(value.begin(), value.end(), [](char c){
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    })){
        throw ParseException("story replay: full lowercase revision required");
    }
}
void check_route_names(const ReplayData& data){
    const JsonObject& routes = data.candidate.to_object_throw().get_object_throw("routes");
    size_t count = 0;
    for (const StoryStep& event : data.row.steps){
        if (event.kind == StoryStepKind::ROUTE){
            routes.get_array_throw(event.name);
            count++;
        }
    }
    if (routes.size() != count){
        throw ParseException("story replay: extra supplied route");
    }
}
void check_route_options(const JsonObject& config){
    const int64_t limit = config.get_integer_throw("max_route_moves");
    if (limit < 1 || limit > 4096){
        throw ParseException("story replay: route move budget outside 1..4096");
    }
    const JsonObject& costs = fields(config.get_value_throw("costs"), {"tile", "turn", "grass", "warp"});
    for (const auto& [key, value] : costs){
        const double number = value.to_double_throw();
        if (!std::isfinite(number) || number < 0 || (key == "tile" && number == 0)){
            throw ParseException("story replay: unsupported route cost");
        }
    }
}
//  An export pose ({"certainty": "tile", "map", "where": {"kind":
//  "tiles", "tiles", "facing"}}) as a grid pose.
JsonValue export_grid_pose(const JsonValue& value){
    const JsonObject& pose = value.to_object_throw("story replay");
    const JsonObject& where = pose.get_object_throw("where");
    JsonObject result;
    result["map"] = pose.get_string_throw("map");
    result["tiles"] = where.get_array_throw("tiles").clone();
    if (where.get_value("facing") != nullptr){
        result["facing"] = where.get_string_throw("facing");
    }
    return result;
}
//  One captured event in the #312 step format: its poses as grid poses, a
//  route's target and a step's direction as args. The export left out a
//  step's direction when it was the default, north.
JsonValue capture_step(const JsonObject& event){
    JsonObject step;
    JsonObject args;
    for (const auto& [key, value] : event){
        if (key == "target"){
            args["target"] = export_grid_pose(value);
        }else if (key == "direction"){
            args["direction"] = value.clone();
        }else if (key == "declared_after"){
            step["declared_after"] = export_grid_pose(value);
        }else{
            step[key] = value.clone();
        }
    }
    if (event.get_string_throw("kind") == "step" && args.get_value("direction") == nullptr){
        args["direction"] = "north";
    }
    if (args.size() != 0){
        step["args"] = std::move(args);
    }
    return step;
}
//  The capture's checkpoint, exported before the #312 step format, read
//  with FRLG's actions. Its native fields aren't read: the replay runs the
//  common checkpoint only.
StoryCheckpoint read_capture(const JsonValue& document){
    const JsonObject& root = document.to_object_throw("story replay");
    expect(root, "format", "pokesim-native-checkpoint-export/1");
    expect(root, "game", "frlg");
    const JsonObject& checkpoint = root.get_object_throw("checkpoint");
    JsonObject converted;
    converted["id"] = checkpoint.get_string_throw("id");
    converted["title"] = checkpoint.get_string_throw("title");
    converted["start"] = export_grid_pose(checkpoint.get_value_throw("start"));
    JsonArray events;
    for (const JsonValue& event : checkpoint.get_array_throw("events")){
        events.push_back(capture_step(event.to_object_throw("story replay")));
    }
    converted["events"] = std::move(events);
    StoryCheckpoint result;
    static_cast<Pokemon::StoryCheckpoint&>(result) = Pokemon::read_story_checkpoint(JsonValue(std::move(converted)), "replay capture", story_action_specs());
    return result;
}
void check_row(const ReplayData& data){
    if (!data.row.start || data.row.steps.empty()){
        throw ParseException("story replay: nonempty checkpoint and start pose required");
    }
    const JsonObject& specs = data.binding.to_object_throw().get_object_throw("capabilities");
    if (specs.size() != data.row.steps.size()){
        throw ParseException("story replay: missing/extra event bindings");
    }
    authored_pose(Pokemon::pose_of(*data.row.start), data.maps);
    for (const StoryStep& event : data.row.steps){
        check_capability(event, specs.get_value_throw(event.name));
        if (!event.skip_if.empty() || !event.action.empty() || event.once_per_save){
            throw ParseException("story replay: unsupported event options");
        }
        if (event.args.contains("target")){
            authored_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(event.args, "target")), data.maps);
        }
        if (event.pose_after){
            authored_pose(Pokemon::pose_of(*event.pose_after), data.maps);
        }
    }
}
} // namespace
OverworldPose read_pose(const JsonValue& value, const std::vector<MapData>& maps){
    const JsonObject& root = fields(value, {"map", "tile", "facing", "elevation"});
    const JsonArray& tile = root.get_array_throw("tile");
    const int64_t level = root.get_integer_throw("elevation");
    if (tile.size() != 2 || level < 0 || level > 15){
        throw ParseException("story replay: expected a known runtime-local pose");
    }
    OverworldPose result{.position = {root.get_string_throw("map"), {coordinate(tile[0]), coordinate(tile[1])}},
        .elevation = (uint8_t)level, .facing = direction(root.get_string_throw("facing"))};
    check_pose(result, maps);
    return result;
}
OverworldPose authored_pose(const Pokemon::Pose& value, const std::vector<MapData>& maps){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(value);
    const auto facing = Pokemon::facing_of(value);
    if (value.certainty != Pokemon::PoseCertainty::TILE || tiles.size() != 1 || !facing){
        throw ParseException("story replay: one authored tile and facing required");
    }
    const MapData& map = map_named(maps, value.map);
    OverworldPose result{.position = {value.map, tiles[0]}, .elevation = elevation_at(map, tiles[0]), .facing = *facing};
    check_pose(result, maps);
    return result;
}
JsonValue pose_json(const OverworldPose& pose, std::optional<Direction> facing){
    JsonObject result;
    result["map"] = pose.position.map;
    JsonArray tile;
    tile.push_back(pose.position.point.x);
    tile.push_back(pose.position.point.y);
    result["tile"] = std::move(tile);
    result["elevation"] = pose.elevation;
    result["facing"] = facing ? JsonValue(direction_name(*facing)) : JsonValue();
    return result;
}
ReplayData read_data(std::string_view capture, std::string_view binding, std::string_view candidate){
    JsonValue config = parse_json(std::string(binding));
    const JsonObject& root = fields(config, {"format", "capture", "map_revision", "maps", "capabilities", "costs", "max_route_moves", "policy"});
    expect(root, "format", "pokesim-story-replay-binding/1");
    check_policy(root.get_value_throw("policy"));
    check_route_options(root);
    revision(root.get_string_throw("map_revision"));
    const JsonObject& pin = fields(root.get_value_throw("capture"), {"file", "sha256", "source_revision"});
    if (pin.get_string_throw("sha256") != digest(capture)){
        throw ParseException("story replay: capture hash mismatch");
    }
    JsonValue document = parse_json(std::string(capture));
    expect(document.to_object_throw(), "source_revision", pin.get_string_throw("source_revision"));
    check_spatial_options(document.to_object_throw());
    const std::string source_revision = pin.get_string_throw("source_revision");
    revision(source_revision);
    JsonValue plan = parse_json(std::string(candidate));
    const JsonObject& route = fields(plan, {"format", "binding_sha256", "checkpoint", "routes"});
    expect(route, "format", "pokesim-route-candidate/1");
    expect(route, "binding_sha256", digest(binding));
    ReplayData result{.row = read_capture(document),
        .maps = read_maps(root.get_object_throw("maps")), .binding = std::move(config),
        .candidate = std::move(plan), .native_unbound = document.to_object_throw().get_value_throw("unbound").clone(), .binding_hash = digest(binding), .source_revision = source_revision};
    expect(result.candidate.to_object_throw(), "checkpoint", result.row.id);
    check_row(result);
    check_route_names(result);
    return result;
}
ReplayScreen between_steps_screen(std::string_view kind){
    using Pokemon::ScreenEvent;
    const Pokemon::ScreenEvents box{ScreenEvent::DIALOG_BOX};
    const Pokemon::ScreenEvents battle{ScreenEvent::BATTLE_STARTED};
    const Pokemon::ScreenEvents black{ScreenEvent::FADE_BLACK};
    ReplayScreen screen;
    if (kind == "dialog"){
        screen.frames = {{33, box}};
        screen.look_ms = 400;
    }else if (kind == "wild-encounter"){
        screen.frames = {{100, Pokemon::ScreenEvents{ScreenEvent::FADE_WHITE}}, {400, {}}, {1500, battle}};
        screen.look_ms = 2000;
    }else if (kind == "trainer-sight"){
        screen.frames = {{100, box}, {2000, black}, {2500, battle}};
        screen.look_ms = 3000;
    }else if (kind == "warp"){
        //  Long enough after the fade for no battle to follow it.
        screen.frames = {{100, black}, {600, {}}};
        screen.look_ms = 9000;
    }else{
        throw ParseException("story replay: unknown interrupt kind");
    }
    return screen;
}
void validate_scenario(const ReplayData& data, const JsonValue& scenario){
    const JsonObject& root = scenario.to_object_throw();
    for (const auto& [name, value] : root){
        if (name != "entry" && name != "signals" && name != "between_steps" && name != "check_actions" && name != "start_flags"){
            throw ParseException("story replay: unknown scenario field");
        }
        if (name == "check_actions"){
            value.to_boolean_throw();
        }
        if (name == "start_flags"){
            for (const JsonValue& flag : value.to_array_throw()){
                flag.to_string_throw();
            }
        }
    }
    const JsonObject& entry = fields(root.get_value_throw("entry"), {"basis", "screen", "pose"});
    expect(entry, "basis", "scripted");
    entry.get_string_throw("screen");
    read_pose(entry.get_value_throw("pose"), data.maps);
    for (const auto& [name, signal] : root.get_object_throw("signals")){
        const auto event = std::find_if(data.row.steps.begin(), data.row.steps.end(), [&](const auto& e){ return e.name == name; });
        if (event == data.row.steps.end()){
            throw ParseException("story replay: signal references unknown event");
        }
        if (event->kind == StoryStepKind::ROUTE){
            const JsonObject& s = fields(signal, {"kind", "from", "direction"});
            expect(s, "kind", "wild-encounter");
            read_pose(s.get_value_throw("from"), data.maps);
            direction(s.get_string_throw("direction"));
        }else if (event->kind == StoryStepKind::STEP){
            expect(fields(signal, {"kind"}), "kind", "unconfirmed-transition");
        }else{
            const JsonObject& s = fields(signal, {"kind", "basis", "screen", "pose", "received"});
            expect(s, "kind", "scene-observation");
            expect(s, "basis", "scripted");
            s.get_string_throw("screen");
            read_pose(s.get_value_throw("pose"), data.maps);
            const auto& spec = data.binding.to_object_throw().get_object_throw("capabilities").get_object_throw(name);
            receipts(s.get_object_throw("received"), &spec.get_object_throw("parameters").get_object_throw("require_received"));
        }
    }
    const JsonObject* between = root.get_object("between_steps");
    if (between == nullptr){
        return;
    }
    for (const auto& [name, shown] : *between){
        if (std::none_of(data.row.steps.begin(), data.row.steps.end(), [&](const auto& e){ return e.name == name; })){
            throw ParseException("story replay: interrupt before an unknown event");
        }
        between_steps_screen(fields(shown, {"kind"}).get_string_throw("kind"));
    }
}
} // namespace StoryReplayInternal
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
