/*  Naming Screen
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Types a name on the in-game naming screen (player, rival, box, Pokemon nickname).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_NamingScreen_H
#define PokemonAutomation_PokemonFRLG_NamingScreen_H

#include <string>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_NamingScreenKeyboard.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


// Types `text` on a naming screen that was just opened (cursor on "A", nothing typed) and
// presses OK. Returns once OK is pressed; what follows (e.g. "So your name is ...?") is up to
// the caller.
// Returns false, without pressing anything, if the naming screen is not on screen.
// Throws UserSetupError if the game can't take `text` (see naming_text_error()).
bool type_name(
    ConsoleHandle& console, ProControllerContext& context,
    const std::string& text, NamingScreenKind kind
);


}
}
}
#endif
