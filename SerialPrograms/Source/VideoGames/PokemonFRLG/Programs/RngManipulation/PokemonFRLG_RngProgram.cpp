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

class ConsolePartyCheckActions : public PartyCheckActions{
public:
    ConsolePartyCheckActions(ConsoleHandle& console, ProControllerContext& context, StartMenuContext menu_context)
        : m_console(console), m_context(context), m_menu_context(menu_context)
    {}

    virtual bool open_party_menu() override{
        try{
            open_party_menu_from_overworld(m_console, m_context, m_menu_context);
        }catch (OperationFailedException&){
            return false;
        }
        m_context.wait_for_all_requests();
        return true;
    }
    virtual PartySlot read_last_occupied_slot() override{
        return detect_last_occupied_party_slot(m_console);
    }

private:
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    StartMenuContext m_menu_context;
};

//  Without a free slot, the Pokemon the program catches or receives goes to
//  the PC and the program reads the wrong Pokemon.
void check_party_has_free_slot(Logger& logger, PartyCheckActions& actions){
    logger.log("Checking for a free party slot...");
    if (!actions.open_party_menu()){
        logger.log("Unable to open the party menu. Skipping the free party slot check.", COLOR_RED);
        return;
    }
    if (actions.read_last_occupied_slot() == PartySlot::SIX){
        throw UserSetupError(
            logger,
            "Your party is full. Leave a party slot free so the program can check the " + Pokemon::STRING_POKEMON + " it catches."
        );
    }
    logger.log("Party has a free slot.");
}

}


void check_party_requirement(
    Logger& logger,
    PartyRequirement requirement,
    PartyCheckActions& actions
){
    switch (requirement){
    case PartyRequirement::NONE:
        break;
    case PartyRequirement::FREE_SLOT:
        check_party_has_free_slot(logger, actions);
        break;
    }
}


void RngProgramInstance::start_program_game_checks(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context
){
    ConsolePartyCheckActions actions(env.console, context, start_menu_context());
    check_party_requirement(env.console.logger(), party_requirement(), actions);
}

}
}
}
