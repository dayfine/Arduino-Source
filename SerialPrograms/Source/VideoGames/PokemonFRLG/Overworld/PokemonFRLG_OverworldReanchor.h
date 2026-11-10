/*  Overworld Re-anchor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Finding the player again after a battle or a message stopped a leg
 *  part-way. The timing of the stop narrows where they are to a few tiles of
 *  the leg (and a trainer who saw them may have stopped them earlier, on a
 *  tile in their sight); this looks for holds to walls, none to three, that
 *  end on the same tile from every one of them, and a way back to the leg's
 *  end that walks the same from there whichever it was. If there are none,
 *  the caller stops: the player is never guessed to be on one of them.
 *
 *  The player is taken to be on the leg's line. Wider sets were tried:
 *  with even one tile to either side, Route 1's people and ledges cut some
 *  hold for some tile, and nothing on its grass legs could be found.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldReanchor_H
#define PokemonAutomation_PokemonFRLG_OverworldReanchor_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldWalkedTiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  A trainer who saw the player and walked up to them: which of `map`'s
//  objects, and the tile next to the player they stand on afterwards.
struct TrainerStop{
    size_t object = 0;
    MapPosition at;

    bool operator==(const TrainerStop& other) const = default;
};

//  One way things may stand after a stop: the player's tile, and the
//  trainer who stopped them there, if one did.
struct StopCandidate{
    MapPosition player;
    std::optional<TrainerStop> trainer;

    bool operator==(const StopCandidate& other) const = default;
};
//  Formats a StopCandidate as a string.
std::string to_string(const StopCandidate& candidate);

//  Where `step`, an open WALK from `start`, may have been stopped:
//    - on the tiles `walked` covers, by a wild battle or a message;
//    - on any tile up to `walked.max` a trainer may see, by that trainer:
//      the player stops as soon as they are seen, and the trainer speaks
//      only after walking up, so later than the timing says.
//  Off the grass a message is taken to be a trainer's wherever one may see
//  the player, so there only the second kind are left, if there are any.
//  `start` not known: worked back from the leg's end. Empty if it can't be.
std::vector<StopCandidate> stop_candidates(
    const std::vector<MapData>& maps,
    const std::optional<MapPosition>& start,
    const RouteStep& step,
    WalkedTiles walked
);

//  A candidate reanchor route step with its cost estimate.
struct Reanchor{
    //  Legs to walls, in order, that bring the player to `found` from every
    //  candidate; none if there was only one. Each but the last ends on a
    //  tile that depends on the candidate; its `end` is one of them.
    std::vector<RouteStep> holds;
    MapPosition found;
    //  From `found` back to the stopped leg's end, walking the same from
    //  there whichever candidate it was (a trainer who walked up stays).
    std::vector<RouteStep> back;
};

//  After `step` stopped part-way, from `candidates`. Nothing if no holds
//  bring them all to one tile, or no way back walks the same from there.
std::optional<Reanchor> plan_reanchor(
    const std::vector<MapData>& maps,
    const std::vector<StopCandidate>& candidates,
    const RouteStep& step,
    const RouteCosts& costs = default_route_costs()
);


//  Registers overworld reanchor unit tests.
void add_tests_OverworldReanchor(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
