/*  Story Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <memory>
#include <set>
#include <utility>
#include "Pokemon_StorySteps.h"

namespace PokemonAutomation{
namespace Pokemon{


const char* story_step_kind_name(StoryStepKind kind){
    switch (kind){
    case StoryStepKind::NEW_GAME:
        return "NEW_GAME";
    case StoryStepKind::OPTIONS:
        return "OPTIONS";
    case StoryStepKind::ROUTE:
        return "ROUTE";
    case StoryStepKind::LEGS:
        return "LEGS";
    case StoryStepKind::STEP:
        return "STEP";
    case StoryStepKind::TALK:
        return "TALK";
    case StoryStepKind::ANSWER:
        return "ANSWER";
    case StoryStepKind::MASH:
        return "MASH";
    case StoryStepKind::BATTLE:
        return "BATTLE";
    case StoryStepKind::TRAIN:
        return "TRAIN";
    case StoryStepKind::HEAL:
        return "HEAL";
    case StoryStepKind::SHOP:
        return "SHOP";
    case StoryStepKind::SAVE:
        return "SAVE";
    case StoryStepKind::READY:
        return "READY";
    case StoryStepKind::GAME:
        return "GAME";
    }
    return "?";
}


Outcome unsupported_step(const StoryStep& step){
    return outcome_failed("unsupported_step", std::format("{} {}", story_step_kind_name(step.kind), step.name));
}

Outcome StoryStepExecutor::run(RunState& state, const StoryStep& step){
    return dispatch_story_step(*this, state, step);
}

bool StoryStepExecutor::holds(const std::string& condition, RunState& state){
    return state.flags.contains(condition);
}

Outcome StoryStepExecutor::new_game(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::options(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::route(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::legs(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::step(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::talk(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::answer(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::mash(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::battle(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::train(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::heal(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::shop(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::save(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::ready(RunState&, const StoryStep& step){
    return unsupported_step(step);
}
Outcome StoryStepExecutor::game(RunState&, const StoryStep& step){
    return unsupported_step(step);
}

Outcome dispatch_story_step(StoryStepExecutor& executor, RunState& state, const StoryStep& step){
    switch (step.kind){
    case StoryStepKind::NEW_GAME:
        return executor.new_game(state, step);
    case StoryStepKind::OPTIONS:
        return executor.options(state, step);
    case StoryStepKind::ROUTE:
        return executor.route(state, step);
    case StoryStepKind::LEGS:
        return executor.legs(state, step);
    case StoryStepKind::STEP:
        return executor.step(state, step);
    case StoryStepKind::TALK:
        return executor.talk(state, step);
    case StoryStepKind::ANSWER:
        return executor.answer(state, step);
    case StoryStepKind::MASH:
        return executor.mash(state, step);
    case StoryStepKind::BATTLE:
        return executor.battle(state, step);
    case StoryStepKind::TRAIN:
        return executor.train(state, step);
    case StoryStepKind::HEAL:
        return executor.heal(state, step);
    case StoryStepKind::SHOP:
        return executor.shop(state, step);
    case StoryStepKind::SAVE:
        return executor.save(state, step);
    case StoryStepKind::READY:
        return executor.ready(state, step);
    case StoryStepKind::GAME:
        return executor.game(state, step);
    }
    return unsupported_step(step);
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
        return executor.run(state, step);
    };
    event.pose_learned = step.pose_after;
    return event;
}

namespace{

//  The first event of a checkpoint that starts from a save: where the save
//  leaves the player, for a run that reloaded it.
Event from_save_event(const Pose& start){
    Event event;
    event.name = "from_save";
    event.why = "where the checkpoint's save leaves the player";
    event.act = [](RunState&){ return outcome_ok(); };
    event.pose_learned = start;
    return event;
}

}

Checkpoint build_story_checkpoint(const StoryCheckpoint& data, StoryStepExecutor& executor){
    Checkpoint checkpoint;
    checkpoint.id = data.id;
    if (data.start){
        checkpoint.events.push_back(from_save_event(*data.start));
    }
    for (const StoryStep& step : data.steps){
        checkpoint.events.push_back(story_event(step, executor));
    }
    return checkpoint;
}


namespace{

//  What a step of its kind must have, or "" if it has it.
std::string missing_field(const StoryStep& step){
    switch (step.kind){
    case StoryStepKind::ROUTE:
    case StoryStepKind::READY:
        return step.target ? "" : "no target";
    case StoryStepKind::LEGS:
        if (step.legs.empty()){
            return "no legs";
        }
        for (const StoryLeg& leg : step.legs){
            if (leg.tiles <= 0){
                return "a leg of no tiles";
            }
        }
        return "";
    case StoryStepKind::TRAIN:
        return step.battles > 0 ? "" : "no battles";
    case StoryStepKind::SHOP:
        return step.items.empty() ? "no items" : "";
    case StoryStepKind::GAME:
        return step.variant.empty() ? "no variant" : "";
    default:
        return "";
    }
}

}

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
            problems.emplace_back(where + ": no name");
        }else if (!names.insert(step.name).second){
            problems.emplace_back(std::format("{}: a second step named {}", where, step.name));
        }
        const std::string missing = missing_field(step);
        if (!missing.empty()){
            problems.emplace_back(std::format("{} {}: {} {}", where, step.name, story_step_kind_name(step.kind), missing));
        }
    }
    return problems;
}


Pose tile_pose(std::string map, std::vector<TilePos> tiles, std::optional<Direction> facing){
    Pose pose;
    pose.map = std::move(map);
    pose.certainty = tiles.empty() ? PoseCertainty::UNKNOWN
        : tiles.size() == 1 ? PoseCertainty::TILE : PoseCertainty::SET;
    pose.where = std::make_shared<const TilePose>(std::move(tiles), facing);
    return pose;
}

std::optional<Direction> facing_of(const Pose& pose){
    const TilePose* tile_pose = dynamic_cast<const TilePose*>(pose.where.get());
    return tile_pose ? tile_pose->facing : std::nullopt;
}



}
}
