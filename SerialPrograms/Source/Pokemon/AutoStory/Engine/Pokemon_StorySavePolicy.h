/*  Story Save Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Who saves at a checkpoint's end, and whether the run is now on that
 *  save. The engine asks its game's policy (StoryGame::save) after each
 *  checkpoint that passed; whether the next checkpoint may reload depends
 *  on the answer. A game that autosaves (PLZA) never saves itself and only
 *  says whether it saw the autosave: autosave_policy(). A game that saves
 *  from its menu brings its own policy.
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySavePolicy_H
#define PokemonAutomation_Pokemon_StorySavePolicy_H

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include "Pokemon/AutoStory/Data/Pokemon_StoryRowExecution.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon_StoryBackend.h"
#include "Pokemon_StoryPoseConcept.h"
#include "Pokemon_ZonePose.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The failure of a policy's answer for a checkpoint whose end the run is
//  not on a save of: no error, the engine logs it as "not saved
//  (<detail>)".
inline constexpr std::string_view NOT_SAVED = "not_saved";
//  NOT_SAVED, `reason` its detail: "save unverified".
inline Outcome outcome_not_saved(std::string reason){
    return outcome_failed(std::string(NOT_SAVED), std::move(reason));
}

//  The save at the end of `checkpoint`, which passed: ok when the run is on
//  a save made there (saving first when the policy saves),
//  outcome_not_saved() when it is not, another failure when a save failed.
template <StoryPose PoseType>
using StorySavePolicy = std::function<Outcome(StoryBackend<PoseType>& backend, const StoryCheckpoint& checkpoint)>;


//  Where a checkpoint's run saw the game's autosave.
struct AutosaveSighting{
    //  The step it first showed in; none: not seen.
    std::optional<std::string> first_step = {};
    //  It showed in the checkpoint's last step.
    bool at_end = false;
};

//  The autosave game's policy: the program never saves. The run is on a
//  save when `sighting` says the game autosaved in the checkpoint's last
//  step ("[Save] <id>: the game autosaved (seen)"). An autosave seen only
//  in an earlier step is not one ("[Save] <id>: the game autosaved in
//  <step>, before <next>: ..."): a reload would skip the steps after it.
//  With none seen, the detail says what `rows` expected: "save unverified",
//  "the plan's autosave was not seen" or "no autosave seen".
template <StoryPose PoseType>
StorySavePolicy<PoseType> autosave_policy(
    StoryRows rows, std::function<AutosaveSighting(const StoryCheckpoint& checkpoint)> sighting
);

//  Instantiated for ZonePose (Pokemon_ZonePose.h), in
//  Pokemon_StorySavePolicy.cpp.
extern template StorySavePolicy<ZonePose> autosave_policy<ZonePose>(
    StoryRows rows, std::function<AutosaveSighting(const StoryCheckpoint& checkpoint)> sighting
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
