/*  BDSP AutoStory - Wait beside a wanderer, on the simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  wait_beside_wanderer() on the grid simulation: the wander wait tests'
 *  body, and the story simulation's wait_for_wanderer step.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_SimWanderWait_H
#define PokemonAutomation_PokemonBDSP_AutoStory_SimWanderWait_H

#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryPose.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_WanderWait.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP_AutoStory_GridSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The wait on the simulation: a tap a tile, A at the faced tile, and the
//  console's time per ask (the dialog watch). An ask talks to a wanderer
//  on the faced tile, or to someone on one of `still` (world tiles): the
//  people the simulation does not walk.
class SimWanderWait : public WanderWaitBody{
public:
    explicit SimWanderWait(GridSim& sim, std::vector<Pokemon::TilePos> still = {});
    virtual bool step(Pokemon::Direction direction) override;
    virtual void face(Pokemon::Direction direction) override;
    virtual bool ask() override;
    virtual void wait(Milliseconds time) override;

private:
    GridSim& m_sim;
    std::vector<Pokemon::TilePos> m_still;
};

//  The wait plan of a wait_for_wanderer step, as the console reads it, in
//  its map's tiles.
WanderWaitPlan wander_wait_plan(const Pokemon::StoryStep& step);

//  Plays wait_for_wanderer `step` on `sim`, on `world`, from the tile of
//  `start` (its map's): the console's wait (wait_for_wanderer()), its
//  failures named as there. Who on the map has no wanderer on `sim`
//  stands on their spawn.
Pokemon::Outcome sim_wait_for_wanderer(GridSim& sim, const StoryWorld& world, const Pokemon::Pose& start, const Pokemon::StoryStep& step);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
