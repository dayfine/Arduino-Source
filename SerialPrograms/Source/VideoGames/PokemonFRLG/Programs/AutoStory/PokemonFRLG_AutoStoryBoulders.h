/*  AutoStory Boulders
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a Strength boulder push does to the story's state, for the
 *  console's push_boulder step and the map-level sim alike.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBoulders_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBoulders_H

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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
