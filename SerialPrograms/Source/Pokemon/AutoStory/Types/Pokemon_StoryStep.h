/*  Story Step
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One step of a tile game's story (FRLG, BDSP) as plain data: its kind,
 *  the action that does it and the action's args. A game's executor does it
 *  (Pokemon_StoryStepExecutor.h); the shared JSON reads and writes it
 *  (Data/Pokemon_StoryStepJson.h).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStep_H
#define PokemonAutomation_Pokemon_StoryStep_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon_GridPose.h"
#include "Pokemon_StoryActionArg.h"
#include "Pokemon_StoryStepKind.h"

namespace PokemonAutomation{
namespace Pokemon{


// Specification of an atomic story task with actions and postconditions.
struct StoryStep{
    StoryStepKind kind = StoryStepKind::TALK;
    std::string name;
    std::string why;

    //  How the step does its kind: one of the built-in actions
    //  (Pokemon_StoryActions.h) or the game's own; for a GAME step, the
    //  game's action. Empty is the kind's default action.
    std::string action;
    //  The arguments the action reads, parsed against its spec when the
    //  game loads the step, with the spec's fallbacks filled in.
    StoryActionArgs args;

    //  Skipped when all of these hold (and there is at least one): names
    //  the game's executor answers (StoryStepExecutor::holds()), run flags
    //  unless the game reads them off the screen.
    std::vector<std::string> skip_if;
    //  Preparation for what follows, done once per save: a TEACH, a check,
    //  a read. Once it has succeeded and changed nothing
    //  (Outcome::changed_nothing), the attempts after a reload of the same
    //  save skip it, so a retry goes from the reload straight to the
    //  battle. One that changed the game (a TM taught) runs again, as the
    //  reload undid it. Only with build_story_checkpoint()'s PreparedSteps.
    bool once_per_save = false;
    //  Where the step leaves the player, when its action does not say: the
    //  end of a scene that moves the player.
    std::optional<GridPose> pose_after;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
