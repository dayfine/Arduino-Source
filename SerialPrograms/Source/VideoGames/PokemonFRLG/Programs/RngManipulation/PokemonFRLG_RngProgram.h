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

#include "Common/Cpp/Logging/AbstractLogger.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartySlot.h"
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


//  The game steps that check_party_requirement() takes. They are injected so
//  that its decisions run in tests without a Switch.
class PartyCheckActions{
public:
    virtual ~PartyCheckActions() = default;

    //  Open the party menu from the overworld. Returns false if it can't.
    virtual bool open_party_menu() = 0;

    //  Read the last occupied slot on the open party menu.
    virtual PartySlot read_last_occupied_slot() = 0;
};

//  Checks that the party meets `requirement`, starting from the overworld.
//  Throws UserSetupError if it doesn't. If the party menu can't be opened,
//  logs that and returns. May leave the party menu open.
void check_party_requirement(
    Logger& logger,
    PartyRequirement requirement,
    PartyCheckActions& actions
);


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
    //  menu open: the program's first reset closes the game. Final so that
    //  every RNG program runs the check its party_requirement() asks for.
    virtual void start_program_game_checks(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context
    ) override final;
};


}
}
}
#endif
