/*  BDSP AutoStory - Story actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <memory>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_StoryActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::StoryFailure;


namespace{

//  Why the backend's inputs throw.
inline constexpr std::string_view NO_BACKEND_INPUT =
    "BDSP's story actions play through StoryStepIO, not the backend's inputs.";

std::optional<StoryFailure> call_handler(
    const BdspStepAction& fn,
    const BdspStep& step,
    BdspRunState& state
){
    if (!fn){
        return StoryFailure{.code = "unsupported_step", .detail = step.action};
    }
    return fn(step, state);
}

std::optional<StoryFailure> dispatch_handler(
    const BdspStoryHandlers& handlers,
    const BdspStep& step,
    BdspRunState& state
){
    if (step.action == "new_game"){
        if (std::get<NewGameStepArgs>(step.args).skip_if_loaded && state.save_loaded){
            return std::nullopt;
        }
        return call_handler(handlers.new_game, step, state);
    }
    if (step.action == "options"){
        return call_handler(handlers.options, step, state);
    }
    if (step.action == "answer"){
        return call_handler(handlers.answer, step, state);
    }
    if (step.action == "battle"){
        return call_handler(handlers.battle, step, state);
    }
    if (step.action == "train"){
        return call_handler(handlers.train, step, state);
    }
    if (step.action == "heal"){
        return call_handler(handlers.heal, step, state);
    }
    if (step.action == "save"){
        return call_handler(handlers.save, step, state);
    }
    if (story_events().contains(step.action) && handlers.event){
        return call_handler(handlers.event(step.action), step, state);
    }
    return StoryFailure{.code = "unsupported_step", .detail = step.action};
}

std::optional<StoryFailure> dispatch_action(
    StoryWalkSteps& walks,
    const BdspStoryHandlers& handlers,
    const BdspStep& step,
    BdspRunState& state
){
    if (step.action == "route"){
        return walks.route(state, step);
    }
    if (step.action == "legs"){
        return walks.legs(state, step);
    }
    if (step.action == "step"){
        return walks.step(state, step);
    }
    if (step.action == "talk"){
        return walks.talk(state, step);
    }
    if (step.action == "scene"){
        return walks.mash(state, step);
    }
    return dispatch_handler(handlers, step, state);
}

Pokemon::StoryStep make_engine_step(
    std::shared_ptr<const StoryCheckpoint> row,
    size_t index,
    StoryWalkSteps& walks,
    BdspRunState& run_state,
    std::shared_ptr<const BdspStoryHandlers> shared_handlers
){
    const BdspStep& src_step = row->steps[index];
    Pokemon::StoryStep step;
    step.name = src_step.name;
    step.why = src_step.why;
    step.action = src_step.action;
    step.run = [row = std::move(row), index, &walks, &run_state, shared_handlers = std::move(shared_handlers)](
        Pokemon::StoryBackend&, Pokemon::RunState&
    ) -> std::optional<StoryFailure>{
        if (index == 0){
            run_state.pose = row->start.value_or(Pokemon::GridPose{});
            walks.begin_attempt(row->id, row->steps);
            if (shared_handlers->on_attempt_start){
                shared_handlers->on_attempt_start(*row);
            }
        }
        const BdspStep& current = row->steps[index];
        const auto run_action = [&](){
            return run_bdsp_action(walks, *shared_handlers, current, run_state);
        };
        if (shared_handlers->wrap_step){
            return shared_handlers->wrap_step(*row, current, run_state, run_action);
        }
        return run_action();
    };
    return step;
}

} // namespace


BdspLogBackend::BdspLogBackend(std::function<void(std::string_view)> log, CancellableScope& scope)
    : m_log(std::move(log))
    , m_scope(scope)
{}

void BdspLogBackend::press(Button, Milliseconds, Milliseconds){
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string(NO_BACKEND_INPUT));
}
void BdspLogBackend::stick(double, Milliseconds, Milliseconds, Pokemon::StoryPace){
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string(NO_BACKEND_INPUT));
}
void BdspLogBackend::wait(Milliseconds){
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string(NO_BACKEND_INPUT));
}

ScreenObservation BdspLogBackend::observe(){
    return {};
}

void BdspLogBackend::log(std::string_view line){
    if (m_log){
        m_log(line);
    }
}

bool is_known_bdsp_action(std::string_view action){
    if (action == "route" || action == "legs" || action == "step" || action == "talk" || action == "scene" ||
        action == "new_game" || action == "options" || action == "answer" || action == "battle" ||
        action == "train" || action == "heal" || action == "save"
    ){
        return true;
    }
    return story_events().contains(std::string(action));
}

std::optional<StoryFailure> run_bdsp_action(
    StoryWalkSteps& walks,
    const BdspStoryHandlers& handlers,
    const BdspStep& step,
    BdspRunState& state
){
    begin_walk_step(walks, step);
    std::optional<StoryFailure> failure = dispatch_action(walks, handlers, step, state);
    if (!failure && step.pose_after){
        state.pose = *step.pose_after;
    }
    return failure;
}

std::vector<Pokemon::StoryCheckpoint> make_bdsp_engine_checkpoints(
    std::span<const StoryCheckpoint> rows,
    StoryWalkSteps& walks,
    BdspRunState& run_state,
    BdspStoryHandlers handlers
){
    auto shared_handlers = std::make_shared<const BdspStoryHandlers>(std::move(handlers));
    std::vector<Pokemon::StoryCheckpoint> out;
    out.reserve(rows.size());

    for (const StoryCheckpoint& src : rows){
        auto row = std::make_shared<const StoryCheckpoint>(src);
        Pokemon::StoryCheckpoint cp;
        cp.id = row->id;
        cp.title = row->title;
        cp.steps.reserve(row->steps.size());
        for (size_t i = 0; i < row->steps.size(); i++){
            cp.steps.push_back(make_engine_step(row, i, walks, run_state, shared_handlers));
        }
        out.push_back(std::move(cp));
    }

    return out;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
