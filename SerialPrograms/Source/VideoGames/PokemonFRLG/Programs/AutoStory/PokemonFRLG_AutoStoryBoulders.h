/*  AutoStory Boulders
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a Strength boulder push and a Cut do to the story's state, for
 *  the console's push_boulder and cut_tree steps and the map-level sim
 *  alike.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBoulders_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBoulders_H

#include <functional>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Pokemon_StoryPose.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_AutoStoryCheckpoints.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Where a PUSH_BOULDER_VARIANT step leaves the player: `tiles` tiles on in
//  its direction, behind the boulder. A boulder that ends on a floor
//  switch's button turns that switch on in `state.gates`.
Pokemon::Outcome boulder_pushed(Pokemon::RunState& state, const StoryStep& step, const std::vector<MapData>& maps);

//  Where a CUT_TREE_VARIANT step leaves the player: on the tree's tile, a
//  tile on in its direction, facing that way. The tree's gate, if the map
//  has one, is open in `state.gates` until the next warp
//  (regrow_on_warp()).
Pokemon::Outcome tree_cut(Pokemon::RunState& state, const StoryStep& step, const std::vector<MapData>& maps);

//  Runs a step (`run`) on `state`; if it left the map it started on, every
//  tree cut grows back in `state.gates`, as a warp clears the trees'
//  temporary hide flags.
Pokemon::Outcome regrow_on_warp(const std::vector<MapData>& maps, const std::function<Pokemon::Outcome()>& run, Pokemon::RunState& state);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
