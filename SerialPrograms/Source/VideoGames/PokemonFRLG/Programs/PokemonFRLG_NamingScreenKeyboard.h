/*  Naming Screen Keyboard
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The in-game keyboard used to name the player, the rival, PC boxes and Pokemon, and a
 *  planner that turns a name into button presses. Pure data and logic: no console needed.
 *
 *  Everything here follows pret/pokefirered src/naming_screen.c at commit
 *  ce6406d8c2eaf46d543c8a2606de0c3cb1951982 (English game). The decomp includes the Switch
 *  revision (REVISION >= 0xA), so the layout is the one on the Switch release.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_NamingScreenKeyboard_H
#define PokemonAutomation_PokemonFRLG_NamingScreenKeyboard_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>
#include "PokemonFRLG_NamingScreenCursor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


// Which naming screen is open; it sets how many characters fit (NamingScreenTemplate.maxChars).
enum class NamingScreenKind : uint8_t{
    PLAYER,     //  PLAYER_NAME_LENGTH
    RIVAL,      //  PLAYER_NAME_LENGTH
    BOX,        //  BOX_NAME_LENGTH
    POKEMON,    //  POKEMON_NAME_LENGTH, for caught Pokemon and the nickname rater
};
//  Maximum character length for the given naming screen type.
size_t naming_screen_max_chars(NamingScreenKind kind);
//  Display name for naming screen kind.
std::string_view naming_screen_kind_name(NamingScreenKind kind);

// Why the game can't take `text` as a name of this kind, or "" if it can: a character with
// no key, more characters than fit, or nothing but spaces (the game then keeps no name).
std::string naming_text_error(const std::string& text, NamingScreenKind kind);

// The presses that type `text` from `start` without confirming it: the shortest moves to
// each character's key, then A. The name stays on screen with OK not pressed, which is where
// SID Helper starts.
// Throws InternalProgramError if naming_text_error() rejects `text`; check that first.
std::vector<NamingAction> plan_naming_keys(
    const std::string& text, NamingScreenKind kind, const NamingCursor& start = NamingCursor()
);

// plan_naming_keys(), then START and A to press OK.
std::vector<NamingAction> plan_naming_input(
    const std::string& text, NamingScreenKind kind, const NamingCursor& start = NamingCursor()
);




} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
