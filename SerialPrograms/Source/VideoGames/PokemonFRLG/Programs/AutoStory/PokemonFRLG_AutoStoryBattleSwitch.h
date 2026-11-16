/*  AutoStory Battle Switch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Sending a party member out from the battle menu through POKEMON and
 *  SHIFT.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleSwitch_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleSwitch_H

#include <cstddef>
#include <string_view>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The party list in battle opens on the lead: down to `slot`, then A, and
//  SHIFT, the top of its menu.
void send_member(ProControllerContext& context, size_t slot);

//  Switches the lead dragged out back in from the battle menu: POKEMON,
//  the party list's slot that reads `lead`, SHIFT. Not the slot out (the
//  top-left one), so never a member the loop didn't mean to send. False,
//  and back at the menu, if the list doesn't show or no other slot reads
//  the lead.
bool switch_lead_in(ConsoleHandle& console, ProControllerContext& context, Language language, std::string_view lead);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
