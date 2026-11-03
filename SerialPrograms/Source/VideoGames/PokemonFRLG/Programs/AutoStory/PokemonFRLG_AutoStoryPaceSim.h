/*  AutoStory Pace Sim
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pacing in a patch (PatchSteps) on the OverworldSim, as the console
 *  paces: `pace` and back, a tap to turn first when the player faces
 *  another way (step_once()). A trainer who sees the player is fought, then
 *  the test's walk back onto the patch runs, as pace_trainer_battle() does
 *  on the console.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPaceSim_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPaceSim_H

#include <functional>
#include <string>
#include <string_view>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryCapture.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class SimPatch : public PatchSteps{
public:
    //  `back_to_patch`: after a trainer's battle, the walk back onto the
    //  patch's tile; none: the player stays where the trainer stopped them.
    SimPatch(OverworldSim& sim, Pokemon::Direction pace, Pokemon::Direction back, std::function<void()> back_to_patch = {});

    virtual bool step() override;
    virtual bool battle_shows() override;
    virtual bool overworld_shows() override;
    virtual bool message_shows() override;
    virtual std::string fight_trainer() override;
    virtual void dump_screen(std::string_view label) override;

private:
    OverworldSim& m_sim;
    Pokemon::Direction m_pace;
    Pokemon::Direction m_back;
    std::function<void()> m_back_to_patch;
    bool m_displaced = false;
};


void add_tests_AutoStoryPaceSim(UnitTestDatabase& database);



}
}
}
#endif
