/*  Story Save Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon_StorySavePolicy.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

int save_mode_rank(StorySaveMode mode){
    switch (mode){
    case StorySaveMode::NONE:
        return 0;
    case StorySaveMode::AFTER_VERIFICATION:
        return 1;
    case StorySaveMode::PER_CHECKPOINT:
        return 2;
    }
    return 0;
}

} // namespace


const EnumStringMap<CheckpointTestingStatus>& CHECKPOINT_TESTING_STATUS_NAMES(){
    static const EnumStringMap<CheckpointTestingStatus> MAP{
        {CheckpointTestingStatus::TESTING, "testing"},
        {CheckpointTestingStatus::VERIFICATION, "verification"},
        {CheckpointTestingStatus::REPRODUCTION, "reproduction"},
    };
    return MAP;
}

StorySaveMode testing_status_save_mode(CheckpointTestingStatus status){
    switch (status){
    case CheckpointTestingStatus::TESTING:
        return StorySaveMode::NONE;
    case CheckpointTestingStatus::VERIFICATION:
        return StorySaveMode::AFTER_VERIFICATION;
    case CheckpointTestingStatus::REPRODUCTION:
        return StorySaveMode::PER_CHECKPOINT;
    }
    return StorySaveMode::NONE;
}

StorySaveMode effective_save_mode(StorySaveMode requested, CheckpointTestingStatus status){
    const StorySaveMode phase_mode = testing_status_save_mode(status);
    return save_mode_rank(requested) <= save_mode_rank(phase_mode) ? requested : phase_mode;
}

StorySavePolicy phase_gated(StorySavePolicy inner, PhaseGateConfig config){
    return [inner = std::move(inner), config = std::move(config)](
        StoryBackend& backend, const StoryCheckpoint& passed
    ) mutable -> std::optional<StoryFailure>{
        StorySaveMode mode = config.requested_mode;
        if (!config.ignore_phase){
            const auto it = config.status_by_id.find(passed.id);
            const CheckpointTestingStatus status = it != config.status_by_id.end()
                ? it->second
                : CheckpointTestingStatus::TESTING;
            mode = effective_save_mode(config.requested_mode, status);
        }
        const SavePoint point = passed.id == config.last_checkpoint_id
            ? SavePoint::RUN_END
            : SavePoint::CHECKPOINT_END;
        if (!story_save_allowed(mode, CheckpointVerdict::PASSED, point)){
            backend.log(std::format(
                "[Checkpoint] {}: not saving (save mode {})",
                passed.id, story_save_mode_name(mode)
            ));
            return std::nullopt;
        }
        if (!inner){
            return std::nullopt;
        }
        return inner(backend, passed);
    };
}



} // namespace Pokemon
} // namespace PokemonAutomation
