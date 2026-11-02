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
    //  Open legs go on the D-pad now, a whole tile a tap, so they land exactly
    //  at any length ("south 5" went 6 in r134a on stick holds). A wall at a
    //  leg's end is still worth a little: r64 went 38 tiles round the town
    //  to end each leg on one, for 12 straight.
    RobustPlanCosts costs;
    costs.exact_open_tiles = costs.max_open_tiles;
    costs.per_open_tile = 1;
    costs.leg_not_at_wall = 4;
    return costs;
}



}
}
}
