/*  BDSP AutoStory Segment 00 Steps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Follows 00.04b's steps from its start on the story maps (the JSON grids
 *  built into the program), the way a first run goes: no Potions yet, and
 *  the Poke Mart clerk's way, which walks back to Route 202 itself. Each
 *  route is planned with the costs its variant names, from the tiles the
 *  step before may leave us on.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/Navigation/Pokemon_MapSet.h"
#include "Pokemon/Navigation/Pokemon_StorySteps.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"
#include "Data/PokemonBDSP_AutoStory_Route202.h"
#include "Data/PokemonBDSP_AutoStory_SandgemTown.h"
#include "Data/PokemonBDSP_AutoStory_Segment00Steps.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Utils/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_Segment00Steps_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;

std::string describe(const std::vector<TilePos>& tiles){
    std::string text;
    for (const TilePos& pos : tiles){
        const std::optional<Pokemon::MapTile> local = story_maps().to_local(pos);
        text += " " + (local ? local->map + " " + Pokemon::to_string(local->pos) : Pokemon::to_string(pos));
    }
    return text;
}

bool contains(const std::vector<TilePos>& tiles, const TilePos& pos){
    return std::find(tiles.begin(), tiles.end(), pos) != tiles.end();
}

//  Plans `step` from `at` and moves `at` to where the plan may end.
std::string plan_route(Logger& logger, std::vector<TilePos>& at, const StoryStep& step){
    const std::optional<Pokemon::RobustPlanCosts> costs = story_route_costs(step.variant);
    if (!costs){
        return step.name + ": no costs named \"" + step.variant + "\".";
    }
    const std::vector<TilePos> goal = Pokemon::tiles_of(*step.target);
    if (goal.empty()){
        return step.name + ": the target has no tiles.";
    }
    const auto plan = Pokemon::plan_robust_route(story_maps().world(), at,
        [&](const TilePos& pos){ return contains(goal, pos); }, *costs);
    if (!plan){
        return step.name + ": no plan from" + describe(at) + ".";
    }
    std::string text = std::format("00.04b {}: {} legs from{}", step.name, plan->size(), describe(at));
    for (const Pokemon::PlannedLeg& leg : *plan){
        text += "\n    " + to_string(leg.leg) + " to" + describe(leg.after);
    }
    logger.log(text);
    if (!plan->empty()){
        at = plan->back().after;
    }
    for (const TilePos& pos : at){
        if (!contains(goal, pos)){
            return step.name + ": the plan may end off the target, at" + describe({pos}) + ".";
        }
    }
    return "";
}

//  Walks `step`'s legs tile by tile from each of `at`, a tile short too
//  when the step allows it.
std::string walk_legs(std::vector<TilePos>& at, const StoryStep& step){
    const Pokemon::TileGrid& world = story_maps().world();
    for (const Pokemon::StoryLeg& leg : step.legs){
        std::vector<TilePos> next;
        for (TilePos pos : at){
            for (int i = 0; i < leg.tiles; i++){
                const std::optional<TilePos> stepped = world.step(pos, leg.direction);
                if (!stepped){
                    return step.name + ": " + direction_name(leg.direction) + " is blocked at" + describe({pos}) + ".";
                }
                if (step.variant == "one_short_ok" && i + 1 == leg.tiles){
                    next.push_back(pos);
                }
                pos = *stepped;
            }
            next.push_back(pos);
        }
        at = std::move(next);
    }
    return "";
}

class Test_AfterCatchSteps : public UnitTest{
public:
    Test_AfterCatchSteps()
        : UnitTest("PokemonBDSP::AutoStory - 00.04b steps: lint, and every route plans on the story maps")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return check(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    static UnitTestResult check(Logger& logger){
        const Pokemon::StoryCheckpoint& data = segment_00_after_catch();
        const std::vector<std::string> problems = Pokemon::lint_story_checkpoint(data);
        if (!problems.empty()){
            return problems[0];
        }
        if (!data.start || Pokemon::tiles_of(*data.start).empty()){
            return "00.04b has no start tiles.";
        }
        const TilePos save_spot = story_maps().to_world(MAP_ROUTE_202, Route202::SAVE_SPOT);
        std::vector<TilePos> at = Pokemon::tiles_of(*data.start);
        for (const StoryStep& step : data.steps){
            std::string error;
            switch (step.kind){
            case StoryStepKind::ROUTE:
                error = plan_route(logger, at, step);
                break;
            case StoryStepKind::LEGS:
                error = walk_legs(at, step);
                break;
            case StoryStepKind::SHOP:
                //  The clerk's way: out of the Mart's door. (The lady's ends
                //  on the save spot, where the save step stands too.)
                at = story_door_front(MAP_SANDGEM_TOWN, SandgemTown::POKE_MART_DOOR);
                break;
            case StoryStepKind::SAVE:
                if (!step.target || step.target->map != MAP_ROUTE_202 || at != std::vector<TilePos>{save_spot}){
                    error = step.name + ": expected a save on Route 202's save spot, from" + describe(at) + ".";
                }
                break;
            default:
                break;
            }
            if (!error.empty()){
                return error;
            }
            if (step.pose_after){
                at = Pokemon::tiles_of(*step.pose_after);
            }
            //  From both save spots at once the planner gives up: the walk
            //  north must start from the one.
            if (step.name == "back_to_route_202" && at != std::vector<TilePos>{save_spot}){
                return step.name + ": expected to end on the save spot.";
            }
        }
        for (const TilePos& pos : at){
            if (!story_at_jubilife_exit(pos)){
                return "00.04b's walk north may end off Jubilife's exit, at" + describe({pos}) + ".";
            }
        }
        return true;
    }
};

}


void add_tests_AutoStory_Segment00Steps(UnitTestDatabase& database){
    database.add<Test_AfterCatchSteps>();
}



}
}
}
