/*  Shared Authored Checkpoint JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_StoryStepJson.h"
#include "Pokemon_AuthoredCheckpointJson.h"

namespace PokemonAutomation{
namespace Pokemon{


void validate_json_fields(
    const JsonObject& object, std::initializer_list<std::string_view> fields, std::string_view source
){
    if (object.size() != fields.size()){
        throw ParseException(std::format("{}: missing or extra authored checkpoint fields.", source));
    }
    for (std::string_view field : fields){
        object.get_value_throw(std::string(field), std::string(source));
    }
}

StoryCheckpoint read_authored_checkpoint(
    const JsonValue& value, std::string_view game, std::string_view source,
    const ExecutionReader& read_execution, std::span<const StoryActionSpec> actions
){
    const JsonObject& root = value.to_object_throw(std::string(source));
    validate_json_fields(root, {"format", "game", "checkpoint", "execution"}, source);
    if (game.empty() || root.get_string_throw("format") != "pokesim-authored-checkpoint/1"
        || root.get_string_throw("game") != game
    ){
        throw ParseException(std::format("{}: unsupported authored checkpoint format/game.", source));
    }
    StoryCheckpoint checkpoint = read_story_checkpoint(root.get_value_throw("checkpoint"), source, actions);
    read_execution(root.get_object_throw("execution", std::string(source)), source);
    return checkpoint;
}

JsonValue authored_checkpoint_json(const StoryCheckpoint& checkpoint, std::string_view game, JsonObject execution){
    if (game.empty()){
        throw ParseException("Authored checkpoint game must not be empty.");
    }
    JsonObject result;
    result["format"] = "pokesim-authored-checkpoint/1";
    result["game"] = std::string(game);
    result["checkpoint"] = story_checkpoint_json(checkpoint);
    result["execution"] = std::move(execution);
    return result;
}

StoryCheckpoint load_authored_checkpoint(
    std::span<const EmbeddedJsonFile> files, std::string_view file, std::string_view game,
    const ExecutionReader& read_execution, std::span<const StoryActionSpec> actions
){
    for (const auto& [name, text] : files){
        if (name == file){
            return read_authored_checkpoint(parse_json(text), game, file, read_execution, actions);
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Authored checkpoint is not embedded: " + std::string(file));
}



} // namespace Pokemon
} // namespace PokemonAutomation
