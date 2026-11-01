/*  Overworld Re-anchor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Finding the player again after a battle or a message stopped a leg
 *  part-way. How far they got can't be timed reliably, and earlier open
 *  legs may have left them a tile or two off to the side too. So this
 *  takes every tile they may be on, near the leg, and looks for holds to
 *  walls, one to three, that end on the same tile from all of them.
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
    //  Legs to walls, in order. Empty when the stop itself told where the
    //  player is. Each but the last ends on a tile that depends on where
    //  the player was; its `end` is one of them.
    std::vector<RouteStep> holds;
    //  Where the player is after them.
    MapPosition end;
};

//  After `step` stopped part-way. `start` is where it started, if known;
//  else it is worked back from its end. `at_sign`: `step`, held to a wall,
//  was stopped again and again by a message right away, as by a sign it
//  walked into. Nothing if no holds find the player.
std::optional<Reanchor> plan_reanchor(
    const std::vector<MapData>& maps,
    const std::optional<MapPosition>& start,
    const RouteStep& step,
    bool at_sign = false
);


void add_tests_OverworldReanchor(UnitTestDatabase& database);



}
}
}
#endif
