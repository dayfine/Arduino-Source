/*  BDSP AutoStory - Table Routes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story table's route steps, each from where the steps before it in
 *  its row leave the player, and the legs the program plans for them: what
 *  the route tests and the route bench check.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_TableRoutes_H
#define PokemonAutomation_PokemonBDSP_AutoStory_TableRoutes_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP_AutoStory_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A route step of the table, and where the steps before it in its row leave
//  the player: the row's start, then each step's target or pose_after. A legs
//  or step action that does not say where it ends leaves the start unknown.
struct TableRoute{
    std::string row;
    const BdspStep* step;
    std::optional<Pokemon::GridPose> from;
    //  The world the step is planned on; none if the start is unknown or no
    //  world joins its map and the target's.
    const StoryWorld* world;
};

//  Every route step of the story table, in table order.
std::vector<TableRoute> table_routes();

//  The world tiles of `pose` on `world`.
std::vector<Pokemon::TilePos> world_tiles(const StoryWorld& world, const Pokemon::GridPose& pose);

//  The legs a table route step plans, as walk_route() plans them: the step's
//  costs, then the default ones; no legs if neither plans.
struct RouteStepPlan{
    std::vector<Pokemon::TilePos> start;
    std::optional<std::vector<PlannedLeg>> legs;
    bool default_costs = false;     //  the step's own costs found no plan
};

//  Plans `route`, which must have a world.
RouteStepPlan plan_table_route(const TableRoute& route);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
