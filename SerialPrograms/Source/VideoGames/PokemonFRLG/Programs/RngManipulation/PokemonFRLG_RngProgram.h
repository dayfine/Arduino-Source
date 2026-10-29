/*  RNG Program
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Base class for the FRLG RNG programs. Each program states what the game
 *  must look like when it starts, and the base class checks it before the
 *  program runs, so a setup mistake stops the run at once instead of failing
 *  in a misleading way many resets later.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_RngProgram_H
#define PokemonAutomation_PokemonFRLG_RngProgram_H

#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


enum class PartyRequirement{
    NONE,
    //  The program catches or receives a Pokemon and reads it from the end of
    //  the party. In a full party it would go to the PC instead.
    FREE_SLOT,
};


class RngProgramInstance : public SingleSwitchProgramInstance{
public:
    using SingleSwitchProgramInstance::SingleSwitchProgramInstance;

    //  What the program needs from the party.
    virtual PartyRequirement party_requirement() const = 0;

    //  The start menu the player sees where the program starts.
    virtual StartMenuContext start_menu_context() const{
        return StartMenuContext::STANDARD;
    }

    //  Runs with the game on the overworld where the player saved. May leave a
    //  menu open: the program's first reset closes the game.
    virtual void start_program_game_checks(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context
    ) override;
};


}
}
}
#endif
