/*  Overworld Re-anchor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Finding the player again after a battle or a message stopped a leg
 *  part-way. How far they got can't be timed reliably, so this takes every
 *  tile of the leg they may be on and looks for holds to walls, one to
 *  three, that end on the same tile from all of them.
 *
 *  The player is taken to be on the leg's line. Wider sets were tried:
 *  with even one tile to either side, Route 1's people and ledges cut some
 *  hold for some tile, and nothing on its grass legs could be found.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldReanchor_H
#define PokemonAutomation_PokemonFRLG_OverworldReanchor_H

#include <optional>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


struct Reanchor{
    //  Legs to walls, in order. Each but the last ends on a tile that
    //  depends on where the player was; its `end` is one of them.
    std::vector<RouteStep> holds;
    //  Where the player is after them.
    MapPosition end;
};

//  After `step`, an open WALK, stopped part-way. `start` is where it
//  started, if known; else it is worked back from its end. Nothing if no
//  holds find the player.
std::optional<Reanchor> plan_reanchor(
    const std::vector<MapData>& maps,
    const std::optional<MapPosition>& start,
    const RouteStep& step
);


void add_tests_OverworldReanchor(UnitTestDatabase& database);



}
}
}
#endif
