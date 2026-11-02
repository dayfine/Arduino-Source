/*  Isolated Hunt Flight Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#ifndef PokemonAutomation_PokemonSV_IsolatedHuntFlight_H
#define PokemonAutomation_PokemonSV_IsolatedHuntFlight_H
#include <string>
#include "CommonFramework/Language.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
class IsolatedFlightActions{
public:
    virtual ~IsolatedFlightActions() = default;
    // Select a candidate and stop at the Yes/No flight confirmation, without accepting it.
    virtual bool open_confirmation() = 0;
    virtual std::string read_destination() = 0;
    virtual bool confirmation_visible() = 0;
    virtual bool confirm_and_wait() = 0;
};
// A generic fly icon or successful landing is not evidence of destination identity.
bool fly_to_isolated_central(IsolatedFlightActions& actions, Language language);
}
}
}
#endif
