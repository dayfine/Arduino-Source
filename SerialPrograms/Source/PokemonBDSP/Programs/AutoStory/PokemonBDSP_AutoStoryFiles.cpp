/*  BDSP Authored Checkpoint Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon/AutoStory/Grid/Pokemon_GridStoryArgs.h"
#include "Actions/PokemonBDSP_AutoStoryEvents.h"
#include "PokemonBDSP_AutoStoryFiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

std::optional<Pokemon::GridPose> read_optional_pose_after(const JsonObject& args, std::string_view source){
    const JsonValue* field = args.get_value("pose_after");
    if (field == nullptr){
        return std::nullopt;
    }
    return Pokemon::read_grid_pose(*field, source);
}

std::vector<Pokemon::StoryLeg> read_positive_legs(const JsonValue& value, std::string_view source){
    std::vector<Pokemon::StoryLeg> legs = Pokemon::read_story_legs(value, source);
    if (legs.empty()){
        throw ParseException(std::format("{}: legs must not be empty", source));
    }
    for (const Pokemon::StoryLeg& leg : legs){
        if (leg.tiles <= 0){
            throw ParseException(std::format("{}: a leg must have positive tiles", source));
        }
    }
    return legs;
}

bool read_bool_default(const JsonObject& args, const std::string& key, bool fallback){
    const JsonValue* field = args.get_value(key);
    return field == nullptr ? fallback : field->to_boolean_throw(key);
}

std::string read_string_default(const JsonObject& args, const std::string& key){
    const JsonValue* field = args.get_value(key);
    return field == nullptr ? std::string() : field->to_string_throw(key);
}

bool is_known_route_variant(std::string_view variant){
    return variant.empty() || variant == "route" || variant == "town" || variant == "overworld" ||
        variant == "indoors" || variant == "caves" || variant == "robust" || variant == "line" ||
        variant == ROUTE_ONTO_TRIGGER || variant == ROUTE_INTO_SCENE;
}

bool is_known_legs_variant(std::string_view variant){
    return variant.empty() || variant == "legs" || variant == "to_wall" || variant == "to_dialogue" ||
        variant == "to_map" || variant == "one_short_ok" || variant == "counted_wall" || variant == "measured";
}

bool is_known_step_variant(std::string_view variant){
    return variant.empty() || variant == "face" || variant == "to_banner" || variant == "or_legs" || variant == "tap";
}

bool is_known_battle_variant(std::string_view variant){
    return variant.empty() || variant == "fight" || variant == "starter" || variant == "doubles";
}

StoryFailurePolicy read_failure_policy(std::string_view value){
    if (value == "reload_and_retry"){
        return StoryFailurePolicy::RELOAD_AND_RETRY;
    }
    if (value == "stop"){
        return StoryFailurePolicy::STOP;
    }
    throw ParseException("Unsupported BDSP failure policy: " + std::string(value));
}

void validate_metadata(const JsonObject& checkpoint, std::string_view source){
    const JsonValue* field = checkpoint.get_value("metadata");
    if (field == nullptr){
        return;
    }
    const JsonObject& metadata = field->to_object_throw(std::string(source));
    Pokemon::reject_unknown_keys(metadata, {"phase"});
    const std::string& phase = metadata.get_string_throw("phase", std::string(source));
    if (phase != "testing" && phase != "verification" && phase != "reproduction"){
        throw ParseException(std::format("{}: unknown phase {}", source, phase));
    }
}

struct ParsedExecution{
    StoryFailurePolicy on_failure = StoryFailurePolicy::RELOAD_AND_RETRY;
    std::string saves_in = {};
    bool dev_only = false;
};

ParsedExecution read_execution(const JsonObject& execution, std::string_view source){
    try{
        Pokemon::reject_unknown_keys(execution, {"on_failure", "saves_in", "dev_only"});
        ParsedExecution result;
        result.on_failure = read_failure_policy(execution.get_string_throw("on_failure"));
        result.saves_in = execution.get_string_throw("saves_in");
        result.dev_only = read_bool_default(execution, "dev_only", false);
        if ((result.on_failure == StoryFailurePolicy::RELOAD_AND_RETRY) == result.saves_in.empty()){
            throw ParseException("BDSP save-card location is required exactly when the checkpoint saves/retries.");
        }
        return result;
    }catch (const ParseException& error){
        throw ParseException(std::format("{}: {}", source, error.message()));
    }
}

const std::vector<std::string>& bdsp_action_names(){
    static const std::vector<std::string> names = []{
        std::vector<std::string> out{
            "route", "legs", "step", "talk", "scene",
            "new_game", "options", "answer", "battle", "train", "heal", "save",
        };
        for (const auto& [name, unused] : story_events()){
            if (std::find(out.begin(), out.end(), name) == out.end()){
                out.push_back(name);
            }
        }
        return out;
    }();
    return names;
}

StepMoveArgs parse_step_move_args(const JsonObject& args){
    Pokemon::reject_unknown_keys(args, {"direction", "variant", "legs", "door_tiles", "pose_after"});
    StepMoveArgs parsed;
    parsed.direction = Pokemon::read_direction(args.get_string_throw("direction"), "direction");
    parsed.variant = read_string_default(args, "variant");
    if (!is_known_step_variant(parsed.variant)){
        throw ParseException(std::format("unknown step variant \"{}\"", parsed.variant));
    }
    if (args.get_value("door_tiles") != nullptr){
        const int64_t door_tiles = args.get_integer_throw("door_tiles");
        if (door_tiles <= 0){
            throw ParseException("step \"door_tiles\" must be positive");
        }
        parsed.door_tiles = (size_t)door_tiles;
    }
    if (const JsonValue* legs_val = args.get_value("legs")){
        parsed.legs = read_positive_legs(*legs_val, "legs");
    }else if (parsed.variant == "or_legs"){
        throw ParseException("step variant \"or_legs\" requires \"legs\"");
    }
    return parsed;
}

std::optional<BdspStepArgs> parse_walk_step_args(std::string_view action, const JsonObject& args){
    if (action == "route"){
        Pokemon::reject_unknown_keys(args, {"target", "fight_wild", "variant", "legs", "pose_after"});
        RouteStepArgs parsed;
        parsed.target = Pokemon::read_grid_pose(args.get_value_throw("target"), "target");
        parsed.fight_wild = read_bool_default(args, "fight_wild", false);
        parsed.variant = read_string_default(args, "variant");
        if (!is_known_route_variant(parsed.variant)){
            throw ParseException(std::format("unknown route variant \"{}\"", parsed.variant));
        }
        if (const JsonValue* legs_val = args.get_value("legs")){
            parsed.legs = read_positive_legs(*legs_val, "legs");
        }else if (parsed.variant == ROUTE_INTO_SCENE){
            throw ParseException("route variant \"into_scene\" requires \"legs\"");
        }
        return parsed;
    }
    if (action == "legs"){
        Pokemon::reject_unknown_keys(args, {"legs", "fight_wild", "variant", "pose_after"});
        LegsStepArgs parsed;
        parsed.legs = read_positive_legs(args.get_value_throw("legs"), "legs");
        parsed.fight_wild = read_bool_default(args, "fight_wild", false);
        parsed.variant = read_string_default(args, "variant");
        if (!is_known_legs_variant(parsed.variant)){
            throw ParseException(std::format("unknown legs variant \"{}\"", parsed.variant));
        }
        return parsed;
    }
    if (action == "step"){
        return parse_step_move_args(args);
    }
    return std::nullopt;
}

BdspStepArgs parse_action_step_args(std::string_view action, const JsonObject& args){
    if (action == "scene"){
        Pokemon::reject_unknown_keys(args, {"variant", "pose_after"});
        const std::string variant = read_string_default(args, "variant");
        if (!variant.empty() && variant != "a"){
            throw ParseException(std::format("unknown scene variant \"{}\"", variant));
        }
        return SceneStepArgs{.press_a = variant == "a"};
    }
    if (action == "battle"){
        Pokemon::reject_unknown_keys(args, {"may_lose", "variant", "pose_after"});
        BattleStepArgs parsed;
        parsed.may_lose = read_bool_default(args, "may_lose", false);
        parsed.variant = read_string_default(args, "variant");
        if (!is_known_battle_variant(parsed.variant)){
            throw ParseException(std::format("unknown battle variant \"{}\"", parsed.variant));
        }
        return parsed;
    }
    if (action == "answer"){
        Pokemon::reject_unknown_keys(args, {"yes", "pose_after"});
        return AnswerStepArgs{.yes = args.get_boolean_throw("yes")};
    }
    if (action == "train"){
        Pokemon::reject_unknown_keys(args, {"battles", "pose_after"});
        const int64_t battles = args.get_integer_throw("battles");
        if (battles <= 0){
            throw ParseException("train \"battles\" must be positive");
        }
        return TrainStepArgs{.battles = battles};
    }
    if (action == "save"){
        Pokemon::reject_unknown_keys(args, {"saves_in", "pose_after"});
        return SaveStepArgs{.saves_in = read_string_default(args, "saves_in")};
    }
    if (action == "new_game"){
        Pokemon::reject_unknown_keys(args, {"skip_if_loaded", "pose_after"});
        return NewGameStepArgs{.skip_if_loaded = read_bool_default(args, "skip_if_loaded", false)};
    }
    if (action == "sandgem_potions"){
        Pokemon::reject_unknown_keys(args, {"items", "pose_after"});
        return ShopStepArgs{.items = Pokemon::read_story_items(args.get_value_throw("items"), "items")};
    }
    if (action == "options" || action == "talk" || action == "heal" || story_events().contains(std::string(action))){
        Pokemon::reject_unknown_keys(args, {"pose_after"});
        return PlainStepArgs{};
    }
    throw ParseException(std::format("unknown BDSP action \"{}\"", action));
}

} // namespace


BdspStep parse_bdsp_step(std::string_view action, const JsonObject& args){
    BdspStep step;
    step.action = std::string(action);
    step.pose_after = read_optional_pose_after(args, action);
    if (std::optional<BdspStepArgs> walk = parse_walk_step_args(action, args)){
        step.args = std::move(*walk);
        return step;
    }
    step.args = parse_action_step_args(action, args);
    return step;
}

Pokemon::StoryActions make_bdsp_action_parsers(std::function<Pokemon::StoryStepRun(BdspStep)> make_run){
    Pokemon::StoryActions actions;
    for (const std::string& name : bdsp_action_names()){
        actions.emplace(name, [name, make_run](const JsonObject& args) -> Pokemon::StoryStepRun{
            return make_run(parse_bdsp_step(name, args));
        });
    }
    return actions;
}

std::vector<StoryCheckpoint> bdsp_rows_for_mode(const BdspStoryData& data, bool developer_mode){
    std::vector<StoryCheckpoint> rows;
    for (const StoryCheckpoint& row : data.checkpoints){
        if (developer_mode || !row.dev_only){
            rows.push_back(row);
        }
    }
    return rows;
}

StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source){
    std::vector<BdspStep> parsed_steps;
    Pokemon::StoryActions actions = make_bdsp_action_parsers([&parsed_steps](BdspStep step) -> Pokemon::StoryStepRun{
        parsed_steps.push_back(std::move(step));
        return {};
    });
    const Pokemon::StoryCheckpoint core = Pokemon::read_story_checkpoint(value, "bdsp", source, actions);

    const JsonObject& root = value.to_object_throw(std::string(source));
    const JsonObject& checkpoint_obj = root.get_object_throw("checkpoint", std::string(source));
    validate_metadata(checkpoint_obj, source);
    const ParsedExecution execution = read_execution(root.get_object_throw("execution", std::string(source)), source);

    StoryCheckpoint result;
    result.id = core.id;
    result.title = core.title;
    if (const JsonValue* start_field = checkpoint_obj.get_value("start")){
        result.start = Pokemon::read_grid_pose(*start_field, source);
    }
    result.on_failure = execution.on_failure;
    result.saves_in = execution.saves_in;
    result.dev_only = execution.dev_only;
    for (size_t i = 0; i < core.steps.size(); i++){
        BdspStep step = std::move(parsed_steps[i]);
        step.name = core.steps[i].name;
        step.why = core.steps[i].why;
        step.action = core.steps[i].action;
        result.steps.push_back(std::move(step));
    }
    return result;
}

JsonValue authored_checkpoint_document(std::string_view file){
    for (const auto& [name, text] : bdsp_checkpoint_json()){
        if (name == file){
            return parse_json(text);
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Authored checkpoint is not embedded: " + std::string(file));
}

StoryCheckpoint load_authored_checkpoint(std::string_view file){
    return read_authored_checkpoint(authored_checkpoint_document(file), file);
}

BdspStoryData load_bdsp_story(std::span<const Pokemon::StoryDevFile> dev_files){
    std::map<std::string, std::string> embedded_by_filename;
    for (const auto& [name, text] : bdsp_checkpoint_json()){
        if (!name.ends_with(".plan")){
            continue;
        }
        const std::string filename = name + ".json";
        embedded_by_filename.emplace(filename, text);
    }

    std::map<std::string, const Pokemon::StoryDevFile*> overrides;
    for (const Pokemon::StoryDevFile& dev : dev_files){
        if (!embedded_by_filename.contains(dev.name)){
            throw ParseException(std::format("Dev checkpoint {} does not match any built-in BDSP plan file.", dev.name));
        }
        overrides[dev.name] = &dev;
    }

    BdspStoryData data;
    for (const auto& [filename, text] : embedded_by_filename){
        StoryCheckpoint builtin = read_authored_checkpoint(parse_json(text), filename);
        const auto dev_it = overrides.find(filename);
        if (dev_it == overrides.end()){
            data.checkpoints.push_back(std::move(builtin));
            continue;
        }
        const Pokemon::StoryDevFile& dev = *dev_it->second;
        StoryCheckpoint replacement = read_authored_checkpoint(parse_json(dev.text), dev.path);
        if (replacement.id != builtin.id){
            throw ParseException(std::format(
                "Dev checkpoint {} changes id from {} to {}.", dev.path, builtin.id, replacement.id
            ));
        }
        data.dev_files.emplace_back(dev.name, dev.path);
        data.checkpoints.push_back(std::move(replacement));
    }

    std::sort(data.checkpoints.begin(), data.checkpoints.end(), [](const StoryCheckpoint& a, const StoryCheckpoint& b){
        return a.id < b.id;
    });
    for (size_t c = 1; c < data.checkpoints.size(); c++){
        if (data.checkpoints[c].id == data.checkpoints[c - 1].id){
            throw ParseException("Two authored checkpoints are " + data.checkpoints[c].id + ".");
        }
    }
    return data;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
