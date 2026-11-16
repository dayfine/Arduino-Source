/*  Overworld Route Costs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_OverworldRouteCosts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


const RouteCosts& default_route_costs(){
    static const RouteCosts COSTS{
        {RouteCost::TILE, 1},
        {RouteCost::TURN, 2},
        {RouteCost::WARP, 30},
        {RouteCost::SURF, 40},
        {RouteCost::GRASS_TILE, 20},
        {RouteCost::OPEN_STOP, 8},
        {RouteCost::OPEN_WILD_STOP, 300},
        {RouteCost::SIGHT_TILE, 0},
        {RouteCost::SIGHT_OVERSHOOT, 0},
        {RouteCost::SIGN_OVERSHOOT, 3},
        {RouteCost::OPEN_WATER_STOP, 1000},
        {RouteCost::OPEN_CHECK, 10},
    };
    return COSTS;
}

int route_cost(const RouteCosts& costs, RouteCost kind){
    auto iter = costs.find(kind);
    if (iter == costs.end()){
        iter = default_route_costs().find(kind);
    }
    return iter->second;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
