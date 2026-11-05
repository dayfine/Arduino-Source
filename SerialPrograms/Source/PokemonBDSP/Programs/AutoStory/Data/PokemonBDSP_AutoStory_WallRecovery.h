/*  BDSP Wall Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's movement adapter for shared native wall convergence. Predictions
 *  do not confirm controller execution; the walker must establish arrival.
 */
#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WallRecovery_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WallRecovery_H

#include "Pokemon/Navigation/Pokemon_WallRecovery.h"
#include "PokemonBDSP_AutoStory_RoutePlanner.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

//  Safe holds for every possible position, within one explicitly placed map.
//  Never anchors against a person, crosses a door/map edge or hops a ledge.
//  Caller must uncover trainer sight/scene hazards absent from StoryWorld.
std::vector<Pokemon::WallAnchor<Pokemon::TilePos, TileLeg>> wall_recovery_candidates(
    const StoryWorld& world, const std::vector<Pokemon::TilePos>& from,
    const Pokemon::WallRecoveryLimits& limits = Pokemon::WallRecoveryLimits()
);

}
}
}
#endif
