/*  Story Save Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Whether a run is on a save made at a passed checkpoint's end.
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySavePolicy_H
#define PokemonAutomation_Pokemon_StorySavePolicy_H

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include "Pokemon/AutoStory/Data/Pokemon_StoryRowExecution.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon_StoryBackend.h"
#include "Pokemon_StoryPoseConcept.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The failure of a policy's answer when the run is not on a save: no error.
inline constexpr std::string_view NOT_SAVED = "not_saved";
//  NOT_SAVED with `reason` as its detail.
inline Outcome outcome_not_saved(std::string reason){
    return outcome_failed(std::string(NOT_SAVED), std::move(reason));
}

//  Saves at the end of `checkpoint`, which passed, if the policy saves: ok
//  when the run is on that save, outcome_not_saved() when not, another
//  failure when a save failed.
template <StoryPose PoseType>
using StorySavePolicy = std::function<Outcome(StoryBackend<PoseType>& backend, const StoryCheckpoint& checkpoint)>;


//  Where a checkpoint's run saw the game's autosave.
struct AutosaveSighting{
    //  The step it first showed in; none: not seen.
    std::optional<std::string> first_step = {};
    //  It showed in the checkpoint's last step.
    bool at_end = false;
};

//  For a game that autosaves: the run is on a save only when the autosave
//  showed in the checkpoint's last step.
template <StoryPose PoseType>
StorySavePolicy<PoseType> autosave_policy(
    StoryRows rows, std::function<AutosaveSighting(const StoryCheckpoint& checkpoint)> sighting
);


template <StoryPose PoseType>
StorySavePolicy<PoseType> autosave_policy(
    StoryRows rows, std::function<AutosaveSighting(const StoryCheckpoint& checkpoint)> sighting
){
    return [rows = std::move(rows), sighting = std::move(sighting)](
        StoryBackend<PoseType>& backend, const StoryCheckpoint& checkpoint
    ){
        const AutosaveSighting seen = sighting(checkpoint);
        if (seen.at_end){
            backend.log(std::format("[Save] {}: the game autosaved (seen)", checkpoint.id));
            return outcome_ok();
        }
        if (seen.first_step){
            //  The step after the one it showed in: a reload would skip it.
            std::string_view next = checkpoint.steps.empty() ? std::string_view() : std::string_view(checkpoint.steps.back().name);
            for (size_t i = 0; i + 1 < checkpoint.steps.size(); i++){
                if (checkpoint.steps[i].name == *seen.first_step){
                    next = checkpoint.steps[i + 1].name;
                    break;
                }
            }
            backend.log(std::format(
                "[Save] {}: the game autosaved in {}, before {}: not saved, a reload would skip {}",
                checkpoint.id, *seen.first_step, next, next
            ));
            return outcome_not_saved("autosaved before the last step");
        }
        if (rows.unverified_autosaves.contains(checkpoint.id)){
            return outcome_not_saved("save unverified");
        }
        if (rows.autosaves.contains(checkpoint.id)){
            return outcome_not_saved("the plan's autosave was not seen");
        }
        return outcome_not_saved("no autosave seen");
    };
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
