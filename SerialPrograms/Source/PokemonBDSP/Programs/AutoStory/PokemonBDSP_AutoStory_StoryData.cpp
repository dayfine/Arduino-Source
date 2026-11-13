/*  BDSP Authored Checkpoint Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Data/Pokemon_AuthoredCheckpointJson.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_StoryData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

Pokemon::StoryArgSpec required(std::string key, Pokemon::StoryArgType type, std::string why){
    return Pokemon::StoryArgSpec{.key = std::move(key), .type = type, .required = true, .fallback = std::nullopt, .why = std::move(why)};
}

Pokemon::StoryArgSpec optional(std::string key, Pokemon::StoryArgType type, Pokemon::StoryArgValue fallback, std::string why){
    return Pokemon::StoryArgSpec{.key = std::move(key), .type = type, .required = false, .fallback = std::move(fallback), .why = std::move(why)};
}

std::vector<Pokemon::StoryActionSpec> make_bdsp_actions(){
    std::vector<Pokemon::StoryActionSpec> actions(
        Pokemon::builtin_story_actions().begin(), Pokemon::builtin_story_actions().end()
    );
    const Pokemon::StoryArgSpec fight_wild = optional(
        "fight_wild", Pokemon::StoryArgType::BOOLEAN, false, "fight wild battles on the way instead of running"
    );
    const Pokemon::StoryArgSpec route_target = required(
        "target", Pokemon::StoryArgType::GRID_POSE, "where to go; with several tiles, any of them"
    );
    const Pokemon::StoryArgSpec legs_arg = required(
        "legs", Pokemon::StoryArgType::LEGS, "the straight legs to walk"
    );
    const Pokemon::StoryArgSpec direction_arg = required(
        "direction", Pokemon::StoryArgType::DIRECTION, "the way to step or face"
    );
    const Pokemon::StoryArgSpec may_lose = optional(
        "may_lose", Pokemon::StoryArgType::BOOLEAN, false, "losing goes on (a rival's first battle)"
    );
    const Pokemon::StoryArgSpec items = required(
        "items", Pokemon::StoryArgType::ITEMS, "the items and how many of each"
    );

    for (std::string_view cost : {"town", "overworld", "indoors", "caves", "robust", "line"}){
        actions.push_back({.kind = Pokemon::StoryStepKind::ROUTE, .name = std::string(cost), .args = {route_target, fight_wild}});
    }
    for (std::string_view act : {"to_wall", "to_dialogue", "to_map", "one_short_ok"}){
        actions.push_back({.kind = Pokemon::StoryStepKind::LEGS, .name = std::string(act), .args = {legs_arg, fight_wild}});
    }
    actions.push_back({.kind = Pokemon::StoryStepKind::STEP, .name = "to_banner", .args = {direction_arg}});
    actions.push_back({.kind = Pokemon::StoryStepKind::TALK, .name = "talk_to_wanderer", .args = {}});
    actions.push_back({.kind = Pokemon::StoryStepKind::MASH, .name = "a", .args = {}});
    actions.push_back({.kind = Pokemon::StoryStepKind::BATTLE, .name = "starter", .args = {may_lose}});
    actions.push_back({.kind = Pokemon::StoryStepKind::BATTLE, .name = "doubles", .args = {may_lose}});
    actions.push_back({.kind = Pokemon::StoryStepKind::SHOP, .name = "sandgem_potions", .args = {items}});

    for (const auto& [name, unused] : story_events()){
        actions.push_back({.kind = Pokemon::StoryStepKind::GAME, .name = name, .args = {}});
    }

    return actions;
}

StoryFailure read_failure(std::string_view value){
    if (value == "reload_and_retry"){
        return StoryFailure::RELOAD_AND_RETRY;
    }
    if (value == "stop"){
        return StoryFailure::STOP;
    }
    throw ParseException("Unsupported BDSP failure policy: " + std::string(value));
}

//  The file without its "metadata", which the shared reader doesn't take
//  (yet): {"phase": testing, verification or reproduction}, how far the
//  checkpoint is trusted to save (the engine's save policy reads it).
JsonValue without_metadata(const JsonValue& value, std::string_view source){
    JsonObject stripped;
    for (const auto& [key, field] : value.to_object_throw(std::string(source))){
        if (key != "metadata"){
            stripped[key] = field.clone();
            continue;
        }
        const JsonObject& metadata = field.to_object_throw(std::string(source));
        for (const auto& [metadata_key, unused] : metadata){
            if (metadata_key != "phase"){
                throw ParseException(std::format("{}: unsupported metadata field {}", source, metadata_key));
            }
        }
        const std::string& phase = metadata.get_string_throw("phase", std::string(source));
        if (phase != "testing" && phase != "verification" && phase != "reproduction"){
            throw ParseException(std::format("{}: unknown phase {}", source, phase));
        }
    }
    return JsonValue(std::move(stripped));
}

} // namespace

std::span<const Pokemon::StoryActionSpec> bdsp_story_actions(){
    static const std::vector<Pokemon::StoryActionSpec> actions = make_bdsp_actions();
    return actions;
}

StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source){
    StoryFailure on_failure = StoryFailure::RELOAD_AND_RETRY;
    std::string saves_in;
    auto read_execution = [&](const JsonObject& execution, std::string_view src){
        Pokemon::validate_json_fields(execution, {"on_failure", "saves_in"}, src);
        on_failure = read_failure(execution.get_string_throw("on_failure"));
        saves_in = execution.get_string_throw("saves_in");
        if ((on_failure == StoryFailure::RELOAD_AND_RETRY) == saves_in.empty()){
            throw ParseException("BDSP save-card location is required exactly when the checkpoint saves/retries.");
        }
    };
    StoryCheckpoint result;
    static_cast<Pokemon::StoryCheckpoint&>(result) = Pokemon::read_authored_checkpoint(
        without_metadata(value, source), "bdsp", source, read_execution, bdsp_story_actions()
    );
    result.on_failure = on_failure;
    result.saves_in = std::move(saves_in);
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

std::vector<StoryCheckpoint> load_authored_story(){
    std::vector<StoryCheckpoint> result;
    for (const auto& [name, text] : bdsp_checkpoint_json()){
        if (name.ends_with(".plan")){
            result.push_back(read_authored_checkpoint(parse_json(text), name));
        }
    }
    std::sort(result.begin(), result.end(), [](const StoryCheckpoint& a, const StoryCheckpoint& b){
        return a.id < b.id;
    });
    for (size_t c = 1; c < result.size(); c++){
        if (result[c].id == result[c - 1].id){
            throw ParseException("Two authored checkpoints are " + result[c].id + ".");
        }
    }
    return result;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
