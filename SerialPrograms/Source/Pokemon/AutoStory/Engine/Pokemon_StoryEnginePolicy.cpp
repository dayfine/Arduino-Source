/*  Story Engine Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Common/Cpp/Cryptography/SHA256.h"
#include "Pokemon_StoryEnginePolicy.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


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


std::string story_steps_hash(const StoryCheckpoint& checkpoint){
    const std::string& text = checkpoint.steps_json;
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
