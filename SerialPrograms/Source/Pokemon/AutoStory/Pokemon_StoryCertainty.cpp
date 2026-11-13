/*  Story Certainty
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_BuiltinStoryActions.h"
#include "Pokemon_StoryStepExecutor.h"
#include "Pokemon_StoryCertainty.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

StoryCertainty kind_certainty(StoryStepKind kind){
    switch (kind){
    case StoryStepKind::ROUTE:
        return {PoseCertainty::TILE, PoseEnd::WALKED};
    case StoryStepKind::LEGS:
        return {PoseCertainty::SET, PoseEnd::KEEPS};
    case StoryStepKind::STEP:
    case StoryStepKind::HEAL:
        return {PoseCertainty::TILE, PoseEnd::DECLARED};
    case StoryStepKind::TALK:
        return {PoseCertainty::TILE, PoseEnd::KEEPS};
    case StoryStepKind::NEW_GAME:
        return {PoseCertainty::UNKNOWN, PoseEnd::DECLARED};
    default:
        return {PoseCertainty::UNKNOWN, PoseEnd::KEEPS};
    }
}

} // namespace


StoryCertainty story_step_certainty(const StoryStep& step, std::span<const StoryActionSpec> specs){
    StoryCertainty out = kind_certainty(step.kind);
    if (const StoryActionSpec* spec = find_story_action(specs, step.kind, step.action)){
        out.needs = spec->certainty.value_or(out).needs;
        out.leaves = spec->certainty.value_or(out).leaves;
    }
    return out;
}

PoseCertainty certainty_after(const StoryStep& step, PoseEnd leaves, PoseCertainty before){
    switch (leaves){
    case PoseEnd::WALKED:
        return PoseCertainty::TILE;
    case PoseEnd::DECLARED:
        return step.pose_after ? pose_of(*step.pose_after).certainty : before;
    case PoseEnd::GAME_SET:
        return step.pose_after ? PoseCertainty::SET : before;
    case PoseEnd::KEEPS:
        //  A declared end the step does not walk onto is where the game
        //  leaves us, unseen.
        return step.pose_after ? PoseCertainty::SET : before;
    }
    return before;
}

PoseCertainty start_certainty(const StoryCheckpoint& data){
    return data.start ? PoseCertainty::SET : PoseCertainty::UNKNOWN;
}

std::vector<std::string> certainty_gaps(const StoryCheckpoint& data, std::span<const StoryActionSpec> specs){
    std::vector<std::string> gaps;
    PoseCertainty have = start_certainty(data);
    for (size_t c = 0; c < data.steps.size(); c++){
        const StoryStep& step = data.steps[c];
        const StoryCertainty certainty = story_step_certainty(step, specs);
        if (!certainty_covers(have, certainty.needs)){
            gaps.push_back(std::format("{} step {} {}: needs {}, has {}",
                data.id, c + 1, step.name, certainty_name(certainty.needs), certainty_name(have)));
        }
        have = certainty_after(step, certainty.leaves, have);
    }
    return gaps;
}



} // namespace Pokemon
} // namespace PokemonAutomation
