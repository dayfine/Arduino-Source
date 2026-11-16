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

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Pokemon_StoryPose.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_BoulderPushReader.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_AutoStoryCheckpoints.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Where a PUSH_BOULDER_VARIANT step leaves the player: `tiles` tiles on in
//  its direction, behind the boulder. A boulder that ends on a floor
//  switch's button turns that switch on in `state.gates`.
Pokemon::Outcome boulder_pushed(Pokemon::RunState& state, const StoryStep& step, const std::vector<MapData>& maps);


//  The step taps of a boulder push, on the console or the sim.
class BoulderPresses{
public:
    virtual ~BoulderPresses() = default;
    //  A step tap `direction`, and what it did.
    virtual BoulderPressRead press(Direction direction) = 0;
    //  After a tap that did not do what the push needs: clears what
    //  stopped it (a wild battle the step before set off). Whether
    //  something was cleared, so the tap may be tried again.
    virtual bool clear_interrupt() = 0;
};

//  How far a push got.
struct BoulderPush{
    //  The tiles the player walked on, each read off the screen.
    int64_t walked = 0;
    //  Why the push stopped short, if it did.
    std::optional<std::string> failure;
};

//  Pushes the boulder ahead of the player `tiles` tiles `direction`. In
//  FRLG a tap into a boulder pushes it while the player walks in place
//  (PokemonFRLG_BoulderPushReader.h), so each tile takes two taps: one
//  that pushes it (the map stays, the boulder moves) and one onto the tile
//  it left (the map scrolls). A tap a tile left the player a tile short of
//  where the plan has them (console B, 2026-11-16, 19.04b push_south).
BoulderPush push_boulder_tiles(Direction direction, int64_t tiles, BoulderPresses& presses);

//  A PUSH_BOULDER_VARIANT step's outcome after `push`: boulder_pushed()
//  if it went the whole way, else failed, with the player where the taps
//  put them, `push.walked` tiles on.
Pokemon::Outcome boulder_push_outcome(const StoryStep& step, const std::vector<MapData>& maps, const BoulderPush& push, Pokemon::RunState& state);

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
