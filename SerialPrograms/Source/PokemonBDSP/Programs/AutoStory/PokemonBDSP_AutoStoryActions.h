/*  BDSP AutoStory - Story actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's story steps bound to the shared story engine (Pokemon_StoryEngine.h,
 *  #648): walking steps (route, legs, step, talk, scene) run on StoryWalkSteps,
 *  and console/event steps run through BdspStoryHandlers, which ConsoleStory
 *  and SimExecutor each bind.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStoryActions_H
#define PokemonAutomation_PokemonBDSP_AutoStoryActions_H

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/CancellableScope.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonBDSP_AutoStoryCheckpoint.h"
#include "PokemonBDSP_AutoStoryWalkSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Does one BDSP step, updating BDSP's own run state (S2).
using BdspStepAction = std::function<std::optional<Pokemon::StoryFailure>(const BdspStep& step, BdspRunState& state)>;

//  The actions and step lifecycle hooks a binding (ConsoleStory or SimExecutor)
//  supplies when compiling BDSP checkpoints into #648 Pokemon::StoryCheckpoints.
struct BdspStoryHandlers{
    BdspStepAction new_game = {};
    BdspStepAction options = {};
    BdspStepAction answer = {};
    BdspStepAction battle = {};
    BdspStepAction train = {};
    BdspStepAction heal = {};
    BdspStepAction save = {};
    //  The action for the story event named `name` (story_events()).
    std::function<BdspStepAction(const std::string& name)> event = {};

    //  Optional hook called at the start of every checkpoint attempt (step 0),
    //  after walks.begin_attempt() and run_state.pose initialization.
    std::function<void(const StoryCheckpoint& row)> on_attempt_start = {};

    //  Optional wrapper around each step's action execution (interrupt clearing,
    //  guards, exception handling, simulation pose checks).
    std::function<std::optional<Pokemon::StoryFailure>(
        const StoryCheckpoint& row,
        const BdspStep& step,
        BdspRunState& state,
        const std::function<std::optional<Pokemon::StoryFailure>()>& run_action
    )> wrap_step = {};
};


//  The shared engine's StoryBackend for BDSP: routes log lines and cancellation
//  scope. Controller input methods throw InternalProgramError because BDSP's
//  actions drive the game through StoryStepIO.
class BdspLogBackend : public Pokemon::StoryBackend{
public:
    //  `scope` must outlive the backend.
    BdspLogBackend(std::function<void(std::string_view)> log, CancellableScope& scope);

    virtual void press(Button button, Milliseconds hold, Milliseconds release) override;
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace) override;
    virtual void wait(Milliseconds duration) override;
    virtual ScreenObservation observe() override;
    virtual void log(std::string_view line) override;
    virtual CancellableScope& scope() override{
        return m_scope;
    }

private:
    std::function<void(std::string_view)> m_log;
    CancellableScope& m_scope;
};


//  Whether `action` is a built-in BDSP action or a registered story_events() name.
bool is_known_bdsp_action(std::string_view action);

//  Dispatches one BDSP step on `walks` / `handlers` and applies `step.pose_after`
//  to `state.pose` on success.
std::optional<Pokemon::StoryFailure> run_bdsp_action(
    StoryWalkSteps& walks,
    const BdspStoryHandlers& handlers,
    const BdspStep& step,
    BdspRunState& state
);

//  Compiles BDSP checkpoints into #648 Pokemon::StoryCheckpoints ready for
//  Pokemon::run_checkpoints(). `walks` and `run_state` must outlive the run.
std::vector<Pokemon::StoryCheckpoint> make_bdsp_engine_checkpoints(
    std::span<const StoryCheckpoint> rows,
    StoryWalkSteps& walks,
    BdspRunState& run_state,
    BdspStoryHandlers handlers
);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
