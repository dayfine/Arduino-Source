/*  Story Step Executor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <set>
#include <utility>
#include <variant>
#include "Pokemon_BuiltinStoryActions.h"
#include "Pokemon_StoryStepExecutor.h"

namespace PokemonAutomation{
namespace Pokemon{


Pose pose_of(const GridPose& pose){
    return tile_pose(pose.map, pose.tiles, pose.facing);
}

GridPose grid_pose_of(const DeclaredPose& pose){
    if (const TileDeclared* tile = std::get_if<TileDeclared>(&pose)){
        return GridPose{.map = tile->map, .tiles = tile->tiles, .facing = tile->facing};
    }
    return GridPose{.map = std::get<ZoneDeclared>(pose).place, .tiles = {}, .facing = std::nullopt};
}

Pose pose_of(const DeclaredPose& pose){
    return pose_of(grid_pose_of(pose));
}

Outcome unsupported_step(const StoryStep& step){
    return outcome_failed("unsupported_step", std::format("{} {}", story_step_kind_name(step.kind), step.name));
}


void StoryStepExecutor::begin_attempt(const StoryCheckpoint&){}

bool StoryStepExecutor::holds(std::string_view condition, RunState& state){
    return state.flags.contains(std::string(condition));
}


Event story_event(const StoryStep& step, StoryStepExecutor& executor){
    Event event;
    event.name = step.name;
    event.why = step.why;
    for (const std::string& name : step.skip_if){
        event.skip_if.push_back(condition(name, [&executor, name](RunState& state){
            return executor.holds(name, state);
        }));
    }
    event.act = [&executor, &step](RunState& state){
        Outcome outcome = executor.run(step, state);
        //  What a step that failed did to the game is not known to the run.
        if (outcome.succeeded()){
            if (step.gates_after){
                apply_gate_change(state.gates, *step.gates_after);
            }
            state.flags.insert(step.flags_after.begin(), step.flags_after.end());
        }
        return outcome;
    };
    if (step.gates_after){
        event.when_skipped = [&step](RunState& state){
            apply_gate_change(state.gates, *step.gates_after);
        };
    }
    if (step.pose_after){
        event.pose_learned = pose_of(*step.pose_after);
    }
    return event;
}

namespace{

//  The first event of a checkpoint that starts from a save: where the save
//  leaves the player, and the gates and flags it has, for a run that
//  reloaded it.
Event from_save_event(const StoryCheckpoint& data){
    Event event;
    event.name = "from_save";
    event.why = "where the checkpoint's save leaves the player";
    //  A checkpoint that does not say what its save has leaves the run's
    //  gates as the steps before it made them.
    event.act = [gates = data.start_gates, flags = data.start_flags](RunState& state){
        if (gates){
            state.gates = *gates;
        }
        state.flags.insert(flags.begin(), flags.end());
        return outcome_ok();
    };
    if (data.start){
        event.pose_learned = pose_of(*data.start);
    }
    return event;
}

} // namespace

namespace{

//  `event`, for a once_per_save step known in `prepared` as `key`: skipped
//  while `prepared` has it, and added to it when it succeeds changing
//  nothing.
void run_once_per_save(std::string key, Event& event, PreparedSteps& prepared){
    event.skip_if.push_back(condition("done_from_this_save", [&prepared, key](RunState&){
        return prepared.contains(key);
    }));
    event.act = [act = std::move(event.act), &prepared, key](RunState& state) mutable{
        Outcome outcome = act(state);
        if (outcome.succeeded() && outcome.changed_nothing){
            prepared.insert(key);
        }
        return outcome;
    };
}

} // namespace

Checkpoint build_story_checkpoint(const StoryCheckpoint& data, StoryStepExecutor& executor, PreparedSteps* prepared){
    executor.begin_attempt(data);
    Checkpoint checkpoint;
    checkpoint.id = data.id;
    if (data.start || data.start_gates || !data.start_flags.empty()){
        checkpoint.events.push_back(from_save_event(data));
    }
    for (size_t index = 0; index < data.steps.size(); index++){
        const StoryStep& step = data.steps[index];
        checkpoint.events.push_back(story_event(step, executor));
        if (StoryPausePoints* pause = executor.pause_points()){
            checkpoint.events.back().act = [
                act = std::move(checkpoint.events.back().act), pause, label = story_step_pause_label(data.id, index, step.name)
            ](RunState& state) mutable{
                pause->before_step(label);
                return act(state);
            };
        }
        if (prepared != nullptr && step.once_per_save){
            run_once_per_save(std::format("{}/{}", data.id, step.name), checkpoint.events.back(), *prepared);
        }
    }
    return checkpoint;
}


namespace{

//  What is wrong with a step's action and args, each as a line: a GAME
//  step without an action, args that don't fit a built-in action's spec,
//  an empty leg.
std::vector<std::string> action_problems(const StoryStep& step){
    if (step.kind == StoryStepKind::GAME){
        return step.action.empty() ? std::vector<std::string>{"no action"} : std::vector<std::string>{};
    }
    const StoryActionSpec* spec = find_story_action(builtin_story_actions(), step.kind, step.action);
    if (spec == nullptr){
        //  A game's own action: the game checks it when it loads the step.
        return {};
    }
    std::vector<std::string> problems = story_args_errors(*spec, step.args);
    const std::vector<StoryLeg>* legs = value_if_at_it<std::vector<StoryLeg>>(step.args, step.args.find("legs"));
    if (legs != nullptr && legs->empty()){
        problems.emplace_back("no legs");
    }
    for (const StoryLeg& leg : legs ? *legs : std::vector<StoryLeg>{}){
        if (leg.tiles <= 0){
            problems.emplace_back("a leg of no tiles");
        }
    }
    return problems;
}

} // namespace

std::vector<std::string> lint_story_checkpoint(const StoryCheckpoint& data){
    std::vector<std::string> problems;
    if (data.id.empty()){
        problems.emplace_back("a checkpoint has no id");
    }
    std::set<std::string> names;
    for (size_t c = 0; c < data.steps.size(); c++){
        const StoryStep& step = data.steps[c];
        const std::string where = std::format("{} step {}", data.id, c + 1);
        if (step.name.empty()){
            problems.emplace_back(std::format("{}: no name", where));
        }else if (!names.insert(step.name).second){
            problems.emplace_back(std::format("{}: a second step named {}", where, step.name));
        }
        for (const std::string& problem : action_problems(step)){
            problems.emplace_back(std::format("{} {}: {} {}", where, step.name, story_step_kind_name(step.kind), problem));
        }
        //  skip_if's conditions must all hold; once per save is one more.
        if (step.once_per_save && !step.skip_if.empty()){
            problems.emplace_back(std::format("{} {}: once per save with skip_if", where, step.name));
        }
    }
    return problems;
}



} // namespace Pokemon
} // namespace PokemonAutomation
