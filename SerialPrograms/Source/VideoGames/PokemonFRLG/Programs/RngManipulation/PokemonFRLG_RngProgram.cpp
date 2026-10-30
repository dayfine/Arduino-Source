/*  RNG Program
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartySlot.h"
#include "PokemonFRLG_RngProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Starting from the overworld, checks that the party has a free slot for the
//  Pokemon the program will catch or receive; without one it goes to the PC
//  and the program reads the wrong Pokemon. Throws UserSetupError if the party
//  is full. If the party menu can't be opened, logs that and returns. Leaves
//  the party menu open.
void check_party_has_free_slot(ConsoleHandle& console, ProControllerContext& context, StartMenuContext menu_context){
    console.log("Checking for a free party slot...");
    try{
        open_party_menu_from_overworld(console, context, menu_context);
    }catch (OperationFailedException&){
        console.log("Unable to open the party menu. Skipping the free party slot check.", COLOR_RED);
        return;
    }
    context.wait_for_all_requests();
    if (detect_last_occupied_party_slot(console) == PartySlot::SIX){
        throw UserSetupError(
            console,
            "Your party is full. Leave a party slot free so the program can check the " + Pokemon::STRING_POKEMON + " it catches."
        );
    }
    console.log("Party has a free slot.");
}

}


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
