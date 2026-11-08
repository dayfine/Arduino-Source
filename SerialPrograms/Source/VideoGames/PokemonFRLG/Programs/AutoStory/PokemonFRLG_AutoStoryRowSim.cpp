/*  AutoStory Row Sim
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include <string>
#include <vector>
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryRowSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The route the program walks for `step`, a ROUTE or LEGS, from `at`:
//  planned, or the legs laid out.
std::optional<std::vector<RouteStep>> walked_route(const StoryPose& at, const StoryStep& step){
    const MapPosition start{at.map, at.tile};
    if (step.kind == StoryStepKind::LEGS){
        return fixed_route(story_maps(), start, step.legs);
    }
    const std::optional<StoryPose> target = story_pose(*step.target);
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    return plan_overworld_route(story_maps(), start, goal, story_route_costs(), at.facing);
}

namespace{

//  A CUT_TREE_VARIANT step: turned to the tree, cut, and a step onto its
//  tile. Moves `at` there; returns the failure, or empty.
std::string cut_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step){
    if (sim.state().facing != step.direction){
        sim.tap(step.direction);
    }
    if (!sim.cut_tree()){
        return "no tree to cut.";
    }
    sim.tap(step.direction);
    at = StoryPose{at.map, at.tile.next(step.direction), step.direction};
    if (!(sim.state().position == MapPosition{at.map, at.tile})){
        return "expected to step onto the tree's tile " + to_string(MapPosition{at.map, at.tile}) + ".";
    }
    return "";
}

//  A hop() or step_onto(): one step, ending where the table says. Moves
//  `at` there; returns the failure, or empty.
std::string step_onto_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step){
    if (sim.state().facing != step.direction){
        sim.tap(step.direction);
    }
    sim.tap(step.direction);
    at = *story_pose(*step.pose_after);
    return sim.state().position == MapPosition{at.map, at.tile} ? "" : "expected to end on " + to_string(MapPosition{at.map, at.tile}) + ".";
}

} // namespace

//  Runs `step` of row `id` on `sim` as the program does: routes and legs
//  by the sim's walker from `at`, where the table says the player stands,
//  fighting a trainer who sees the player on the way; a hop down a ledge
//  (hop()) or a step onto the tile before one (step_onto()), to where the
//  table says; a step one tap at a
//  time, or for a step into a trainer's line by step_until_seen(), each
//  watched for the trainer; a battle from the trainer's message, which
//  must be up; a cut tree cut and stepped onto. Moves `at` to where the
//  table says the step ends. Returns the failure, or empty.
std::string row_step_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id){
    const std::string where = id + "/" + step.name + ": ";
    const auto on = [&sim]{ return " On " + to_string(sim.state().position) + "."; };
    if (step.kind == StoryStepKind::LEGS || step.kind == StoryStepKind::ROUTE){
        const std::optional<std::vector<RouteStep>> route = walked_route(at, step);
        if (route && route->empty()){
            //  Already there.
            return "";
        }
        const RoutePlanning planning{.maps = &story_maps(), .costs = story_route_costs(), .start = MapPosition{at.map, at.tile}, .label = step.name};
        const std::string failure = route ? OverworldSimWalker(sim).run(*route, planning) : "no legs.";
        at = route ? StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after} : at;
        return failure.empty() ? "" : where + failure + on();
    }
    if (step.kind == StoryStepKind::STEP && step.variant.empty() && step.pose_after){
        const std::string failure = step_onto_on_sim(sim, at, step);
        return failure.empty() ? "" : where + failure + on();
    }
    if (step.kind == StoryStepKind::STEP){
        const auto tap = [&sim, &step]{
            if (sim.state().facing != step.direction){
                sim.tap(step.direction);
            }
            sim.tap(step.direction);
        };
        const auto seen = [&sim]{
            return sim.watch({SimWatcher::MESSAGE, SimWatcher::BATTLE_SCREEN}, TRAINER_LINE_WATCH).has_value();
        };
        const int tiles = step.variant == TRAINER_LINE_VARIANT ? TRAINER_LINE_TILES : 1;
        return step_until_seen(tiles, tap, seen) ? "" : where + "no trainer saw the player." + on();
    }
    if (step.kind == StoryStepKind::GAME && step.variant == CUT_TREE_VARIANT){
        const std::string failure = cut_on_sim(sim, at, step);
        return failure.empty() ? "" : where + failure + on();
    }
    if (step.kind == StoryStepKind::BATTLE){
        if (!sim.close_message() || sim.handle_battle(SimBattleChoice::FIGHT) != SimBattleEnd::ENDED){
            return where + "no_battle." + on();
        }
        at = *story_pose(*step.pose_after);
        if (!(sim.state().position == MapPosition{at.map, at.tile})){
            return where + "expected " + to_string(MapPosition{at.map, at.tile}) + "." + on();
        }
    }
    return "";
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
