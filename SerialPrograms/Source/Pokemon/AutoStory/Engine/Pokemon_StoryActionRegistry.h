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


//  The failure of a console_only action (StoryActionSpec) on a backend that
//  is not a console. Every attempt would fail the same way, so the engine
//  does not reload for it.
inline constexpr std::string_view CONSOLE_ONLY = "console_only";

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

    //  check() for each step of `checkpoint`, its step names (none empty,
    //  none repeated), and its declared poses (its start, each step's
    //  declared_after): each must be the kind the game's pose type
    //  declares (check_declared()). In a game with no tiles, a tile arg
    //  (DIRECTION, LEGS, GRID_POSE) in a step or in one of the game's
    //  actions fails too.
    std::vector<std::string> lint(const StoryCheckpoint& checkpoint) const;

    //  The pause points of this game's run (StoryGame::breakpoints and
    //  pause_point). Mutable state of the run, not of the game's actions.
    StoryPausePoints& pause_points() const{
        return m_pause;
    }

    //  Runs `step`'s action with its args' fallbacks filled in; a step with
    //  no registered action fails as unsupported_step(), a console_only
    //  action on another backend as CONSOLE_ONLY, before any input.
    Outcome dispatch(StoryBackend<PoseType>& backend, const StoryStep& step, RunState& state) const;

private:
    //  The action of `kind` named `name`; for an empty name, the kind's
    //  first. Null when there is none.
    const StoryAction<PoseType>* find(StoryStepKind kind, std::string_view name) const;

    const StoryGame<PoseType>& m_game;
    std::vector<StoryAction<PoseType>> m_actions;
    std::vector<StoryActionSpec> m_specs;
    //  What is wrong with the game's own actions, reported with every lint.
    std::vector<std::string> m_game_problems;
    mutable StoryPausePoints m_pause;
};

//  Instantiated once, in Pokemon_StoryActionRegistry.cpp.
extern template class StoryActionRegistry<GridPose>;
//  And for ZonePose (Pokemon_ZonePose.h).
extern template class StoryActionRegistry<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
