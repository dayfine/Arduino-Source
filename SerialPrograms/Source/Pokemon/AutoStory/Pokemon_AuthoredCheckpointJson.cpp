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


void require_story_json_fields(
    const JsonObject& object, std::initializer_list<std::string_view> fields, std::string_view source
){
    if (object.size() != fields.size()){
        throw ParseException(std::format("{}: missing or extra authored checkpoint fields.", source));
    }
    for (std::string_view field : fields){
        object.get_value_throw(std::string(field), std::string(source));
    }
}

AuthoredCheckpoint read_authored_checkpoint(
    const JsonValue& value, std::string_view expected_game, std::string_view source
){
    const JsonObject& root = value.to_object_throw(std::string(source));
    require_story_json_fields(root, {"format", "game", "checkpoint", "execution"}, source);
    if (expected_game.empty() || root.get_string_throw("format") != "pokesim-authored-checkpoint/1"
        || root.get_string_throw("game") != expected_game
    ){
        throw ParseException(std::format("{}: unsupported authored checkpoint format/game.", source));
    }
    return {
        .checkpoint = read_story_checkpoint(root.get_value_throw("checkpoint"), source),
        .execution = root.get_object_throw("execution", std::string(source)).clone(),
    };
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

AuthoredCheckpoint load_authored_checkpoint(
    std::span<const std::pair<std::string, std::string>> files, std::string_view file, std::string_view expected_game
){
    for (const auto& [name, text] : files){
        if (name == file){
            return read_authored_checkpoint(parse_json(text), expected_game, file);
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Authored checkpoint is not embedded: " + std::string(file));
}



} // namespace Pokemon
} // namespace PokemonAutomation
