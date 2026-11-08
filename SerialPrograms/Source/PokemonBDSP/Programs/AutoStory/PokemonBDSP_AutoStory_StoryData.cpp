/*  BDSP authored story data
 *  From: https://github.com/PokemonAutomation/
 */
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_AuthoredCheckpointJson.h"
#include "PokemonBDSP_AutoStory_StoryData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

StoryFailure read_failure(const std::string& value){
    if (value == "reload_and_retry"){
        return StoryFailure::RELOAD_AND_RETRY;
    }
    if (value == "stop"){
        return StoryFailure::STOP;
    }
    throw ParseException("Unsupported BDSP failure policy: " + value);
}

StoryCheckpoint apply_execution(Pokemon::AuthoredCheckpoint parsed, std::string_view source){
    const JsonObject& execution = parsed.execution;
    Pokemon::require_story_json_fields(execution, {"on_failure", "saves_in"}, source);
    StoryCheckpoint result;
    static_cast<Pokemon::StoryCheckpoint&>(result) = std::move(parsed.checkpoint);
    result.on_failure = read_failure(execution.get_string_throw("on_failure"));
    result.saves_in = execution.get_string_throw("saves_in");
    if ((result.on_failure == StoryFailure::RELOAD_AND_RETRY) == result.saves_in.empty()){
        throw ParseException("BDSP save-card location is required exactly when the checkpoint saves/retries.");
    }
    return result;
}

} // namespace

StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source){
    return apply_execution(Pokemon::read_authored_checkpoint(value, "bdsp", source), source);
}

StoryCheckpoint load_authored_checkpoint(std::string_view file){
    return apply_execution(Pokemon::load_authored_checkpoint(bdsp_checkpoint_json(), file, "bdsp"), file);
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
