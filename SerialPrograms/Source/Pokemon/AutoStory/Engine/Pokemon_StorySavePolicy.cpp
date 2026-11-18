/*  Story Save Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Pokemon_StorySavePolicy.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  The name of the step of `checkpoint` after `step`, if there is one.
std::optional<std::string_view> step_after(const StoryCheckpoint& checkpoint, std::string_view step){
    for (size_t i = 0; i + 1 < checkpoint.steps.size(); i++){
        if (checkpoint.steps[i].name == step){
            return checkpoint.steps[i + 1].name;
        }
    }
    return std::nullopt;
}

} // namespace


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
            const std::string_view last_step = checkpoint.steps.empty() ? std::string_view() : std::string_view(checkpoint.steps.back().name);
            const std::string_view next = step_after(checkpoint, *seen.first_step).value_or(last_step);
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


template StorySavePolicy<ZonePose> autosave_policy<ZonePose>(
    StoryRows rows, std::function<AutosaveSighting(const StoryCheckpoint& checkpoint)> sighting
);



} // namespace Pokemon
} // namespace PokemonAutomation
