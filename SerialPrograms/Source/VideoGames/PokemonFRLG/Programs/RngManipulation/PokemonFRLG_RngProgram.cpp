/*  RNG Program
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_RngNavigation.h"
#include "PokemonFRLG_RngProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


void RngProgramInstance::start_program_game_checks(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context
){
    switch (party_requirement()){
    case PartyRequirement::NONE:
        break;
    case PartyRequirement::FREE_SLOT:
        check_party_has_free_slot(env.console, context, start_menu_context());
        break;
    }
}


}
}
}
