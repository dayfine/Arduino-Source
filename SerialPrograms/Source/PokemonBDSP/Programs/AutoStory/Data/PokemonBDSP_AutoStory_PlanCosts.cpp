/*  BDSP AutoStory - Plan Costs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_PlanCosts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



RobustPlanCosts route_plan_costs(){
    RobustPlanCosts costs;
    costs.exact_open_tiles = costs.max_open_tiles;
    costs.per_open_tile = 2;
    costs.leg_not_at_wall = 6;
    return costs;
}

RobustPlanCosts waypoint_plan_costs(){
    RobustPlanCosts costs;
    costs.exact_open_tiles = costs.max_open_tiles;
    costs.per_open_tile = 0;
    costs.leg_not_at_wall = 0;
    return costs;
}

RobustPlanCosts town_plan_costs(){
    RobustPlanCosts costs;
    //  A tile's hold moves exactly a tile; a longer open leg may end a tile
    //  either way ("south 5" went 6 in r134a), so the plan ends on a wall.
    costs.exact_open_tiles = 1;
    costs.open_error_per = 8;
    costs.leg_not_at_wall = 6;
    costs.per_open_tile = 2;
    return costs;
}



}
}
}
