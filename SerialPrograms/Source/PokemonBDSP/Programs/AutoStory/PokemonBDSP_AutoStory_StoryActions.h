/*  BDSP AutoStory - Story actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's story steps as the actions of the shared story engine
 *  (Pokemon/AutoStory/Engine/): one StoryGame, run by the engine's step
 *  runner (StoryEngineSteps), which the console (ConsoleStory) and the
 *  simulation (SimExecutor) each bind. The walking steps (ROUTE, LEGS,
 *  STEP, TALK, MASH) are the same code on both: StoryWalkSteps on the
 *  binding's StoryStepIO. The steps that need the console (the intro, the
 *  options, answers, battles, training, the Center, saves, and the story's
 *  events, story_events()) are the binding's own: played on the console,
 *  stood in for on the simulation.
 *
 *  An action is found by its step's kind and name, as the engine finds it;
 *  a story event's name finds its event for a step of any kind. A GAME
 *  step runs only the event its variant names, and BDSP has no SHOP or
 *  READY step: the engine's built-in ones fail as unsupported_step().
 *
 *  BDSP's actions drive the game through StoryStepIO, not through the
 *  engine backend's inputs yet: StoryLogBackend gives the runner the log,
 *  the scope and what it plays on. The engine has no step guard and no
 *  look between steps yet: the bindings run each step inside
 *  TableExecutor's (Pokemon_StoryActions.h), whose run_action() is
 *  StoryEngineSteps::act(). This header leaves out the engine's StoryGame,
 *  whose StoryAction would clash with that header's.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryActions_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryActions_H

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/CancellableScope.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Does `step`, from the run so far.
using StoryBoundAction = std::function<Pokemon::Outcome(const Pokemon::StoryStep& step, Pokemon::RunState& state)>;

//  The actions a binding does its own way, one per kind that needs the
//  console.
struct StoryBoundActions{
    StoryBoundAction new_game = {};
    StoryBoundAction options = {};
    StoryBoundAction answer = {};
    StoryBoundAction battle = {};
    StoryBoundAction train = {};
    StoryBoundAction heal = {};
    StoryBoundAction save = {};
    //  The action for the story event named `name` (story_events()).
    std::function<StoryBoundAction(const std::string& name)> event = {};
};


//  The engine's backend for BDSP: the log, the scope and the kind. Its
//  inputs throw InternalProgramError and observe() sees nothing: BDSP's
//  actions play through StoryStepIO.
//  code-health: allow public_methods -- these are the engine's StoryBackend overrides, which the engine fixes; none can move out
class StoryLogBackend : public Pokemon::StoryBackend<Pokemon::GridPose>{
public:
    //  `scope` must outlive the backend.
    StoryLogBackend(Pokemon::StoryBackendKind kind, std::function<void(std::string_view)> log, CancellableScope& scope);

    virtual Pokemon::StoryBackendKind kind() const override{
        return m_kind;
    }
    virtual void press(Button button, Milliseconds hold, Milliseconds release) override;
    virtual void tap(Pokemon::Direction direction) override;
    virtual void hold(Pokemon::Direction direction, Milliseconds duration, Pokemon::StoryPace pace) override;
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace) override;
    virtual void wait(Milliseconds duration) override;
    virtual Pokemon::Observation<Pokemon::GridPose> observe() override;
    virtual Pokemon::Outcome battle(const Pokemon::StoryActionArgs& args) override;
    virtual void log(std::string_view line) override;
    virtual CancellableScope& scope() override{
        return m_scope;
    }

private:
    Pokemon::StoryBackendKind m_kind;
    std::function<void(std::string_view)> m_log;
    CancellableScope& m_scope;
};


//  BDSP's story actions on the engine: the walking steps on `walks`, the
//  rest `bound`'s, run by the engine's step runner on `backend`. `walks`
//  and `backend` must outlive it.
class StoryEngineSteps{
public:
    StoryEngineSteps(StoryWalkSteps& walks, const StoryBoundActions& bound, Pokemon::StoryBackend<Pokemon::GridPose>& backend);
    ~StoryEngineSteps();
    StoryEngineSteps(const StoryEngineSteps&) = delete;
    StoryEngineSteps& operator=(const StoryEngineSteps&) = delete;

    //  `step`'s action (StoryStepRunner::act()): unsupported_step() for a
    //  step with none.
    Pokemon::Outcome act(const Pokemon::StoryStep& step, Pokemon::RunState& state);

    //  What is wrong with `step` before it runs (StoryActionRegistry::check()):
    //  no action for it, or args that don't fit its action's.
    std::vector<std::string> check(const Pokemon::StoryStep& step) const;

private:
    struct Engine;
    std::unique_ptr<Engine> m_engine;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
