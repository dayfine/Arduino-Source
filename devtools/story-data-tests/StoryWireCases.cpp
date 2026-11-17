/*  Story Wire Cases
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The C++ side has no story JSON writer, so this test turns each record
 *  read back into the wire form the case file expects, with defaults
 *  omitted as the Python reader omits them, and compares the two as JSON.
 *
 */

#include <array>
#include <format>
#include <iostream>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include "Common/Compiler.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "Pokemon/AutoStory/Types/Pokemon_DeclaredPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpointMetadata.h"
#include "Pokemon/Navigation/Pokemon_Direction.h"
#include "StoryWireCases.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  The wire names of the step kinds, as Pokemon_StoryStepJson.cpp reads them.
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

//  The wire names of the arg types, in StoryArgType's order.
constexpr std::array<std::string_view, 10> ARG_TYPES{
    "boolean", "integer", "word", "direction", "grid_pose",
    "legs", "words", "items", "integer_rows", "timed_legs",
};
static_assert(ARG_TYPES.size() == (size_t)StoryArgType::TIMED_LEGS + 1, "one wire name per StoryArgType");


std::string_view kind_name(StoryStepKind kind){
    for (const auto& [value, name] : KINDS){
        if (value == kind){
            return name;
        }
    }
    throw ParseException("Step kind with no wire name.");
}

StoryStepKind kind_of(std::string_view name){
    for (const auto& [value, label] : KINDS){
        if (label == name){
            return value;
        }
    }
    throw ParseException(std::format("Unknown step kind {}.", name));
}

StoryArgType arg_type_of(std::string_view name){
    for (size_t i = 0; i < ARG_TYPES.size(); i++){
        if (ARG_TYPES[i] == name){
            return (StoryArgType)i;
        }
    }
    throw ParseException(std::format("Unknown arg type {}.", name));
}


JsonValue strings_json(const std::vector<std::string>& list){
    JsonArray out;
    for (const std::string& item : list){
        out.push_back(JsonValue(item));
    }
    return JsonValue(std::move(out));
}

JsonValue items_json(const std::vector<StoryItem>& list){
    JsonArray out;
    for (const StoryItem& item : list){
        JsonObject row;
        row["item"] = JsonValue(item.item);
        row["quantity"] = JsonValue((int64_t)item.quantity);
        out.push_back(JsonValue(std::move(row)));
    }
    return JsonValue(std::move(out));
}

JsonValue tile_pose_json(const std::string& map, const std::vector<TilePos>& tiles, const std::optional<Direction>& facing){
    JsonArray tile_list;
    for (const TilePos& tile : tiles){
        JsonArray pair;
        pair.push_back(JsonValue((int64_t)tile.x));
        pair.push_back(JsonValue((int64_t)tile.y));
        tile_list.push_back(JsonValue(std::move(pair)));
    }
    JsonObject out;
    out["map"] = JsonValue(map);
    out["tiles"] = JsonValue(std::move(tile_list));
    if (facing){
        out["facing"] = JsonValue(direction_name(*facing));
    }
    return JsonValue(std::move(out));
}

JsonValue grid_pose_json(const GridPose& pose){
    return tile_pose_json(pose.map, pose.tiles, pose.facing);
}

JsonValue declared_pose_json(const DeclaredPose& pose){
    if (const TileDeclared* tile = std::get_if<TileDeclared>(&pose)){
        return tile_pose_json(tile->map, tile->tiles, tile->facing);
    }
    const ZoneDeclared& zone = std::get<ZoneDeclared>(pose);
    JsonObject out;
    out["place"] = JsonValue(zone.place);
    if (zone.spot){
        out["spot"] = JsonValue(*zone.spot);
    }
    if (zone.heading){
        out["heading"] = JsonValue((int64_t)*zone.heading);
    }
    return JsonValue(std::move(out));
}


//  An arg value in its wire form.
struct ArgJson{
    JsonValue operator()(bool value) const{
        return JsonValue(value);
    }
    JsonValue operator()(int64_t value) const{
        return JsonValue(value);
    }
    JsonValue operator()(const std::string& value) const{
        return JsonValue(value);
    }
    JsonValue operator()(Direction value) const{
        return JsonValue(direction_name(value));
    }
    JsonValue operator()(const GridPose& value) const{
        return grid_pose_json(value);
    }
    JsonValue operator()(const std::vector<StoryLeg>& value) const{
        JsonArray out;
        for (const StoryLeg& leg : value){
            JsonObject row;
            row["direction"] = JsonValue(direction_name(leg.direction));
            row["tiles"] = JsonValue((int64_t)leg.tiles);
            out.push_back(JsonValue(std::move(row)));
        }
        return JsonValue(std::move(out));
    }
    JsonValue operator()(const std::vector<std::string>& value) const{
        return strings_json(value);
    }
    JsonValue operator()(const std::vector<StoryItem>& value) const{
        return items_json(value);
    }
    JsonValue operator()(const std::vector<std::vector<int64_t>>& value) const{
        JsonArray out;
        for (const std::vector<int64_t>& row : value){
            JsonArray numbers;
            for (int64_t number : row){
                numbers.push_back(JsonValue(number));
            }
            out.push_back(JsonValue(std::move(numbers)));
        }
        return JsonValue(std::move(out));
    }
    JsonValue operator()(const std::vector<StoryTimedLeg>& value) const{
        JsonArray out;
        for (const StoryTimedLeg& leg : value){
            JsonArray pair;
            pair.push_back(JsonValue((int64_t)leg.turn));
            pair.push_back(JsonValue((int64_t)leg.ms));
            out.push_back(JsonValue(std::move(pair)));
        }
        return JsonValue(std::move(out));
    }
};

JsonValue step_json(const StoryStep& step){
    JsonObject out;
    out["id"] = JsonValue(step.name);
    out["kind"] = JsonValue(std::string(kind_name(step.kind)));
    if (!step.why.empty()){
        out["why"] = JsonValue(step.why);
    }
    if (!step.action.empty()){
        out["action"] = JsonValue(step.action);
    }
    if (!step.args.empty()){
        JsonObject args;
        for (const auto& [key, value] : step.args){
            args[key] = std::visit(ArgJson{}, value);
        }
        out["args"] = JsonValue(std::move(args));
    }
    if (!step.skip_if.empty()){
        out["skip_if"] = strings_json(step.skip_if);
    }
    if (step.pose_after){
        out["declared_after"] = declared_pose_json(*step.pose_after);
    }
    if (step.once_per_save){
        out["once_per_save"] = JsonValue(true);
    }
    return JsonValue(std::move(out));
}

JsonObject verified_json(const StoryVerification& verified){
    JsonObject out;
    out["steps_hash"] = JsonValue(verified.steps_hash);
    if (!verified.build.empty()){
        out["build"] = JsonValue(verified.build);
    }
    if (!verified.date.empty()){
        out["date"] = JsonValue(verified.date);
    }
    if (!verified.runs.empty()){
        out["runs"] = JsonValue(verified.runs);
    }
    return out;
}

JsonObject postconditions_json(const StoryPostconditions& postconditions){
    JsonObject out;
    if (postconditions.position){
        out["position"] = grid_pose_json(*postconditions.position);
    }
    if (!postconditions.items.empty()){
        out["items"] = items_json(postconditions.items);
    }
    if (!postconditions.flags.empty()){
        out["flags"] = strings_json(postconditions.flags);
    }
    return out;
}

JsonObject metadata_json(const StoryCheckpoint& checkpoint){
    JsonObject out;
    if (checkpoint.metadata.phase != StoryPhase::TESTING){
        out["phase"] = JsonValue(story_phase_name(checkpoint.metadata.phase));
    }
    if (checkpoint.metadata.verified){
        out["verified"] = JsonValue(verified_json(*checkpoint.metadata.verified));
    }
    if (!checkpoint.metadata.source.empty()){
        out["source"] = JsonValue(checkpoint.metadata.source);
    }
    JsonObject postconditions = postconditions_json(checkpoint.postconditions);
    if (!postconditions.empty()){
        out["postconditions"] = JsonValue(std::move(postconditions));
    }
    return out;
}

JsonValue checkpoint_json(const StoryCheckpoint& checkpoint){
    JsonObject out;
    out["id"] = JsonValue(checkpoint.id);
    out["title"] = JsonValue(checkpoint.title);
    if (checkpoint.start){
        out["start"] = declared_pose_json(*checkpoint.start);
    }
    JsonArray events;
    for (const StoryStep& step : checkpoint.steps){
        events.push_back(step_json(step));
    }
    out["events"] = JsonValue(std::move(events));
    JsonObject metadata = metadata_json(checkpoint);
    if (!metadata.empty()){
        out["metadata"] = JsonValue(std::move(metadata));
    }
    return JsonValue(std::move(out));
}


//  An arg's fallback, read as its type through a one-arg step.
StoryArgValue read_fallback(const StoryArgSpec& spec, const JsonValue& fallback){
    const std::vector<StoryActionSpec> one{{
        .kind = StoryStepKind::GAME,
        .name = "fallback",
        .args = {{.key = spec.key, .type = spec.type, .required = true, .fallback = std::nullopt, .why = ""}},
    }};
    JsonObject args;
    args[spec.key] = fallback.clone();
    JsonObject step;
    step["id"] = JsonValue("fallback");
    step["kind"] = JsonValue("game_action");
    step["action"] = JsonValue("fallback");
    step["args"] = JsonValue(std::move(args));
    return read_story_step(JsonValue(std::move(step)), "fallback", one).args.at(spec.key);
}

StoryArgSpec read_arg_spec(const JsonObject& arg){
    StoryArgSpec spec{
        .key = arg.get_string_throw("key"),
        .type = arg_type_of(arg.get_string_throw("type")),
        .required = false,
        .fallback = std::nullopt,
        .why = "",
    };
    if (const JsonValue* required = arg.get_value("required")){
        spec.required = required->to_boolean_throw();
    }
    if (const JsonValue* fallback = arg.get_value("fallback")){
        spec.fallback = read_fallback(spec, *fallback);
    }
    return spec;
}

//  The built-in actions plus the case file's.
std::vector<StoryActionSpec> read_actions(const JsonArray& list){
    std::vector<StoryActionSpec> actions(builtin_story_actions().begin(), builtin_story_actions().end());
    for (const JsonValue& item : list){
        const JsonObject& action = item.to_object_throw();
        StoryActionSpec spec{.kind = kind_of(action.get_string_throw("kind")), .name = action.get_string_throw("name"), .args = {}};
        for (const JsonValue& arg : action.get_array_throw("args")){
            spec.args.push_back(read_arg_spec(arg.to_object_throw()));
        }
        actions.push_back(std::move(spec));
    }
    return actions;
}


//  The record `value` reads as, in wire form; throws ParseException if
//  the reader rejects it.
JsonValue read_record(std::string_view record, const JsonValue& value, std::string_view name, std::span<const StoryActionSpec> actions){
    if (record == "pose"){
        return grid_pose_json(read_grid_pose(value, name));
    }
    if (record == "declared"){
        return declared_pose_json(read_declared_pose(value, name));
    }
    if (record == "step"){
        return step_json(read_story_step(value, name, actions));
    }
    if (record == "checkpoint"){
        return checkpoint_json(read_story_checkpoint(value, name, actions));
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("Unknown record {}.", record));
}

//  Empty if the case passes, else why it failed.
std::string check_case(const JsonObject& test, std::span<const StoryActionSpec> actions){
    const JsonObject& input = test.get_object_throw("input");
    const JsonObject& expectation = test.get_object_throw("expectation");
    const std::string name = input.get_string_throw("name");
    const JsonValue* rejected = expectation.get_value("rejected");
    const bool want_rejected = rejected != nullptr && rejected->to_boolean_throw();
    //  The case's own fields are read outside the try: a malformed case file
    //  throws and fails the run, it is not a reader rejection.
    const std::string record = input.get_string_throw("record");
    const JsonValue& value = input.get_value_throw("value");
    JsonValue read;
    try{
        read = read_record(record, value, name, actions);
    }catch (const ParseException& error){
        return want_rejected ? "" : std::format("Unexpected parse failure: {}", error.message());
    }
    if (want_rejected){
        return std::format("Expected rejection, read {}", read.dump(-1));
    }
    const JsonValue& expected = expectation.get_value_throw("read");
    if (read != expected){
        return std::format("Read {}, expected {}", read.dump(-1), expected.dump(-1));
    }
    return "";
}


} // namespace


size_t check_story_wire_cases(const std::string& path){
    const JsonValue root = parse_json(file_to_string(path));
    const JsonObject& file = root.to_object_throw(path);
    const std::vector<StoryActionSpec> actions = read_actions(file.get_array_throw("actions"));
    size_t total = 0;
    size_t failed = 0;
    for (const JsonValue& item : file.get_array_throw("cases")){
        const JsonObject& test = item.to_object_throw(path);
        const std::string error = check_case(test, actions);
        std::cout << test.get_object_throw("input").get_string_throw("name") << ": " << (error.empty() ? "PASS" : error) << '\n';
        total++;
        failed += !error.empty();
    }
    std::cout << std::format("{} of {} shared wire cases passed.", total - failed, total) << '\n';
    return failed;
}



} // namespace Pokemon
} // namespace PokemonAutomation
