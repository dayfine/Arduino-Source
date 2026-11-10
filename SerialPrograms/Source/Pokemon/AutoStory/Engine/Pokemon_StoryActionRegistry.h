/*  Story Action Registry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A game's actions, looked up by a step's kind and action name: the
 *  shared built-ins (Pokemon_StoryBuiltinActions.h) with the game's own on
 *  top. It checks a step against its action's spec before anything runs,
 *  and runs a step's action with the spec's fallbacks filled in.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryActionRegistry_H
#define PokemonAutomation_Pokemon_StoryActionRegistry_H

#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "Pokemon_StoryGame.h"

namespace PokemonAutomation{
namespace Pokemon{


//  `game`'s actions. `game` must outlive the registry.
template <StoryPose PoseType>
class StoryActionRegistry{
public:
    explicit StoryActionRegistry(const StoryGame<PoseType>& game);
    StoryActionRegistry(const StoryActionRegistry&) = delete;
    StoryActionRegistry& operator=(const StoryActionRegistry&) = delete;

    const StoryGame<PoseType>& game() const{
        return m_game;
    }

    //  Every action's spec, for the JSON reader (read_story_checkpoint()).
    const std::vector<StoryActionSpec>& specs() const{
        return m_specs;
    }

    //  What is wrong with `step` before it runs: an action that is not
    //  registered, args that don't fit its spec, a walk of no legs.
    std::vector<std::string> check(const StoryStep& step) const;

    //  check() for each step of `checkpoint`, and its step names: none
    //  empty, none repeated.
    std::vector<std::string> lint(const StoryCheckpoint& checkpoint) const;

    //  Runs `step`'s action with its args' fallbacks filled in; a step with
    //  no registered action fails as unsupported_step().
    Outcome dispatch(StoryBackend<PoseType>& backend, RunState& state, const StoryStep& step) const;

private:
    //  The action of `kind` named `name`; for an empty name, the kind's
    //  first. Null when there is none.
    const StoryAction<PoseType>* find(StoryStepKind kind, std::string_view name) const;

    const StoryGame<PoseType>& m_game;
    std::vector<StoryAction<PoseType>> m_actions;
    std::vector<StoryActionSpec> m_specs;
};

//  Instantiated once, in Pokemon_StoryActionRegistry.cpp.
extern template class StoryActionRegistry<GridPose>;
//  The same for a zone game (PLZA).
extern template class StoryActionRegistry<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
