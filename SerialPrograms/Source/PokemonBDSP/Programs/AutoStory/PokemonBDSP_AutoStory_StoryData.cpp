/*  BDSP authored story data
 *  From: https://github.com/PokemonAutomation/
 */
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepJson.h"
#include "PokemonBDSP_AutoStory_StoryData.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace{
void exact_fields(const JsonObject& value, std::initializer_list<std::string_view> names){
    if (value.size() != names.size()){
        throw ParseException("BDSP authored checkpoint: missing or extra fields.");
    }
    for (std::string_view name : names){
        value.get_value_throw(std::string(name));
    }
}
StoryFailure read_failure(const std::string& value){
    if (value == "reload_and_retry"){
        return StoryFailure::RELOAD_AND_RETRY;
    }
    if (value == "stop"){
        return StoryFailure::STOP;
    }
    throw ParseException("Unsupported BDSP failure policy: " + value);
}
} // namespace
StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source){
    const JsonObject& root = value.to_object_throw(std::string(source));
    exact_fields(root, {"format", "game", "checkpoint", "execution"});
    if (root.get_string_throw("format") != "pokesim-authored-checkpoint/1" || root.get_string_throw("game") != "bdsp"){
        throw ParseException("Unsupported BDSP authored format/game.");
    }
    const JsonObject& execution = root.get_object_throw("execution");
    exact_fields(execution, {"on_failure", "saves_in"});
    StoryCheckpoint result;
    static_cast<Pokemon::StoryCheckpoint&>(result) = Pokemon::read_story_checkpoint(root.get_value_throw("checkpoint"), source);
    result.on_failure = read_failure(execution.get_string_throw("on_failure"));
    result.saves_in = execution.get_string_throw("saves_in");
    if ((result.on_failure == StoryFailure::RELOAD_AND_RETRY) == result.saves_in.empty()){
        throw ParseException("BDSP save-card location is required exactly when the checkpoint saves/retries.");
    }
    return result;
}
StoryCheckpoint load_authored_checkpoint(std::string_view file){
    for (const auto& [name, text] : bdsp_checkpoint_json()){
        if (name == file){
            return read_authored_checkpoint(parse_json(text), file);
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "BDSP authored checkpoint is not embedded: " + std::string(file));
}
} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
