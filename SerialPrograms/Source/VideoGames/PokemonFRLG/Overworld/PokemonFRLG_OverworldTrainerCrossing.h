/*  Overworld Trainer Crossing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a leg crosses the sight of its maps' trainers, and how far one
 *  may walk up to the player.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldTrainerCrossing_H
#define PokemonAutomation_PokemonFRLG_OverworldTrainerCrossing_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Where a WALK leg crosses the sight of its maps' trainers
//  (trainer_may_see()): the tiles of the leg after its start that one may
//  see, in order, and the farthest any of them walks up to the player
//  (their sight), to watch for them that long. Trainers already beaten are
//  counted too: the maps don't know them.
struct TrainerCrossing{
    std::vector<MapPosition> seen;
    int approach_tiles = 0;
};
//  Nothing if `step`, from `start`, crosses no trainer's sight.
std::optional<TrainerCrossing> trainer_crossing(
    const std::vector<MapData>& maps, const MapPosition& start, const RouteStep& step
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
