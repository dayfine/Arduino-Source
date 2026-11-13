/*  Native Offline Story Replay
 *  From: https://github.com/PokemonAutomation/
 */
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "PokemonFRLG_StoryReplay_Executor.h"
#include "PokemonFRLG_StoryReplay.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
const std::string& story_replay_fixture(std::string_view name){
    for (const auto& [file, text] : story_replay_fixture_json()){
        if (name == file){
            return text;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("Unknown replay fixture {}", name));
}
JsonValue replay_story_checkpoint(std::string_view capture, std::string_view binding,
    std::string_view candidate, const JsonValue& scenario){
    const auto data = StoryReplayInternal::read_data(capture, binding, candidate);
    StoryReplayInternal::validate_scenario(data, scenario);
    StoryReplayInternal::ReplayExecutor executor(data, scenario);
    Pokemon::Checkpoint checkpoint = executor.build();
    const auto entry = StoryReplayInternal::read_pose(scenario.to_object_throw().get_object_throw("entry").get_value_throw("pose"), data.maps);
    Pokemon::RunState state;
    state.pose = Pokemon::tile_pose(entry.position.map, {entry.position.point}, entry.facing);
    //  The run flags the checkpoint starts with (card_key_held opens card-key doors).
    if (const JsonArray* flags = scenario.to_object_throw().get_array("start_flags")){
        for (const JsonValue& flag : *flags){
            state.flags.insert(flag.to_string_throw());
        }
    }
    JsonArray logs;
    Pokemon::EventLogger logger = [&](const std::string& line){ logs.push_back(line); };
    const Pokemon::CheckpointReport result = Pokemon::run_checkpoint(state, checkpoint, logger);
    return executor.report(result, state, logs);
}
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
