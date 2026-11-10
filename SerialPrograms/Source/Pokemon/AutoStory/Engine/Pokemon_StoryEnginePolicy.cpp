/*  Story Engine Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Common/Cpp/Cryptography/SHA256.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon_StoryEnginePolicy.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  `step` as it runs: its action named, its args' fallbacks filled in, no why.
StoryStep as_run(const StoryStep& step, std::span<const StoryActionSpec> specs){
    StoryStep result = step;
    result.why.clear();
    if (const StoryActionSpec* spec = find_story_action(specs, step.kind, step.action)){
        result.action = spec->name;
        result.args = with_fallbacks(*spec, std::move(result.args));
    }
    return result;
}

StorySaveMode mode_of(StoryPhase phase){
    switch (phase){
    case StoryPhase::TESTING:
        return StorySaveMode::NONE;
    case StoryPhase::VERIFICATION:
        return StorySaveMode::AFTER_VERIFICATION;
    case StoryPhase::REPRODUCTION:
        return StorySaveMode::PER_CHECKPOINT;
    }
    return StorySaveMode::NONE;
}


} // namespace


std::string story_steps_hash(const StoryCheckpoint& checkpoint, std::span<const StoryActionSpec> specs){
    JsonArray steps;
    for (const StoryStep& step : checkpoint.steps){
        steps.push_back(story_step_json(as_run(step, specs)));
    }
    const std::string text = JsonValue(std::move(steps)).dump(0);
    SHA256 hash;
    hash.push(text.data(), text.size());
    hash.finish();
    return std::format("sha256:{}", hash.get_hash_hex());
}

StorySaveDecision decide_story_save(const StoryCheckpoint& checkpoint, std::string_view steps_hash){
    const StoryCheckpointMetadata& metadata = checkpoint.metadata;
    const std::string phase = story_phase_name(metadata.phase);
    if (metadata.phase == StoryPhase::TESTING){
        return {.mode = StorySaveMode::NONE, .reason = std::format("[Save] {}: phase testing: save mode none", checkpoint.id)};
    }
    if (!metadata.verified || metadata.verified->steps_hash != steps_hash){
        return {
            .mode = StorySaveMode::NONE,
            .reason = std::format(
                "[Save] {}: phase {} but steps_hash mismatch (recorded {}, now {}): treated as testing, save mode none",
                checkpoint.id, phase, metadata.verified ? metadata.verified->steps_hash : "none", steps_hash
            ),
        };
    }
    const StorySaveMode mode = mode_of(metadata.phase);
    return {.mode = mode, .reason = std::format("[Save] {}: phase {}: save mode {}", checkpoint.id, phase, story_save_mode_name(mode))};
}



} // namespace Pokemon
} // namespace PokemonAutomation
