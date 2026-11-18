/*  Story Action Registry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <set>
#include <utility>
#include <variant>
#include "Pokemon/AutoStory/Pokemon_MapUtil.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon_StoryActionRegistry.h"
#include "Pokemon_StoryBuiltinActions.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  A walk of no legs, or a leg of no tiles.
inline std::vector<std::string> leg_problems(const StoryActionArgs& args){
    const std::vector<StoryLeg>* legs = value_if_at_it<std::vector<StoryLeg>>(args, args.find("legs"));
    if (legs == nullptr){
        return {};
    }
    std::vector<std::string> problems;
    if (legs->empty()){
        problems.emplace_back("no legs");
    }
    for (const StoryLeg& leg : *legs){
        if (leg.tiles <= 0){
            problems.emplace_back("a leg of no tiles");
        }
    }
    return problems;
}


//  Why `declared` is no pose of a game whose poses are `tile` ones or not.
inline std::string undeclarable(const DeclaredPose& declared, bool tile){
    const std::string name = std::holds_alternative<TileDeclared>(declared)
        ? std::get<TileDeclared>(declared).map
        : std::get<ZoneDeclared>(declared).place;
    return tile
        ? std::format("\"{}\" is a place, but this game's poses are tiles: declare a map, tiles and a facing", name)
        : std::format("\"{}\" is a map, but this game has no tiles: declare a place, a spot and a heading", name);
}

//  Whether an arg of `type` only means something on a tile grid.
inline bool tile_arg(StoryArgType type){
    return type == StoryArgType::DIRECTION || type == StoryArgType::LEGS || type == StoryArgType::GRID_POSE;
}

//  Each tile arg among `game`'s own actions, for a game with no tiles.
template <StoryPose PoseType>
std::vector<std::string> tile_arg_problems(const StoryGame<PoseType>& game){
    std::vector<std::string> problems;
    if constexpr (!StoryPoseTraits<PoseType>::TILE_MOVES){
        for (const StoryAction<PoseType>& action : game.actions){
            for (const StoryArgSpec& arg : action.spec.args){
                if (tile_arg(arg.type)){
                    problems.push_back(std::format(
                        "action {} takes a tile argument \"{}\", but this game has no tiles", action.spec.name, arg.key
                    ));
                }
            }
        }
    }
    return problems;
}


} // namespace


template <StoryPose PoseType>
StoryActionRegistry<PoseType>::StoryActionRegistry(const StoryGame<PoseType>& game)
    : m_game(game)
    , m_actions(builtin_story_handlers<PoseType>())
    , m_game_problems(tile_arg_problems(game))
    , m_pause(game.breakpoints, game.pause_point)
{
    //  A game's action replaces the built-in of its kind and name in place,
    //  so a kind's default stays the built-in one's position.
    for (const StoryAction<PoseType>& action : game.actions){
        const auto same = std::find_if(m_actions.begin(), m_actions.end(), [&](const StoryAction<PoseType>& builtin){
            return builtin.spec.kind == action.spec.kind && builtin.spec.name == action.spec.name;
        });
        if (same != m_actions.end()){
            *same = action;
        }else{
            m_actions.push_back(action);
        }
    }
    for (const StoryAction<PoseType>& action : m_actions){
        m_specs.push_back(action.spec);
    }
}

template <StoryPose PoseType>
const StoryAction<PoseType>* StoryActionRegistry<PoseType>::find(StoryStepKind kind, std::string_view name) const{
    for (const StoryAction<PoseType>& action : m_actions){
        if (action.spec.kind == kind && (name.empty() || action.spec.name == name)){
            return &action;
        }
    }
    return nullptr;
}

template <StoryPose PoseType>
std::vector<std::string> StoryActionRegistry<PoseType>::check(const StoryStep& step) const{
    if (step.kind == StoryStepKind::GAME && step.action.empty()){
        return {"no action"};
    }
    const StoryAction<PoseType>* action = find(step.kind, step.action);
    if (action == nullptr){
        return {std::format("unknown action \"{}\" for {}", step.action, story_step_kind_name(step.kind))};
    }
    std::vector<std::string> problems = story_args_errors(action->spec, step.args);
    for (std::string& problem : leg_problems(step.args)){
        problems.push_back(std::move(problem));
    }
    if constexpr (!StoryPoseTraits<PoseType>::TILE_MOVES){
        for (const auto& [key, value] : step.args){
            if (tile_arg(story_arg_type(value))){
                problems.push_back(std::format("argument \"{}\" is a tile one, but this game has no tiles", key));
            }
        }
    }
    return problems;
}

template <StoryPose PoseType>
std::vector<std::string> StoryActionRegistry<PoseType>::lint(const StoryCheckpoint& checkpoint) const{
    constexpr bool TILE = StoryPoseTraits<PoseType>::TILE_MOVES;
    std::vector<std::string> problems = m_game_problems;
    if (checkpoint.id.empty()){
        problems.emplace_back("a checkpoint has no id");
    }
    if (checkpoint.start && !check_declared<PoseType>(*checkpoint.start)){
        problems.push_back(std::format("{} start: {}", checkpoint.id, undeclarable(*checkpoint.start, TILE)));
    }
    std::set<std::string> names;
    for (size_t c = 0; c < checkpoint.steps.size(); c++){
        const StoryStep& step = checkpoint.steps[c];
        const std::string where = std::format("{} step {} {}", checkpoint.id, c + 1, step.name);
        if (step.name.empty() || !names.insert(step.name).second){
            problems.push_back(std::format("{}: empty or repeated name", where));
        }
        for (const std::string& problem : check(step)){
            problems.push_back(std::format("{}: {}", where, problem));
        }
        if (step.pose_after && !check_declared<PoseType>(*step.pose_after)){
            problems.push_back(std::format("{}: declared_after: {}", where, undeclarable(*step.pose_after, TILE)));
        }
    }
    return problems;
}

template <StoryPose PoseType>
Outcome StoryActionRegistry<PoseType>::dispatch(StoryBackend<PoseType>& backend, const StoryStep& step, RunState& state) const{
    const StoryAction<PoseType>* action = find(step.kind, step.action);
    if (action == nullptr || !action->handler){
        return unsupported_step(step);
    }
    if (action->spec.console_only && backend.kind() != StoryBackendKind::CONSOLE){
        return outcome_failed(std::string(CONSOLE_ONLY), std::format(
            "{} runs only on a console, not on a {} backend", action->spec.name, story_backend_kind_name(backend.kind())
        ));
    }
    const StoryActionArgs args = with_fallbacks(action->spec, step.args);
    return action->handler(backend, StoryActionCall{.step = step, .args = args, .pause = m_pause}, state);
}


template class StoryActionRegistry<GridPose>;
template class StoryActionRegistry<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
