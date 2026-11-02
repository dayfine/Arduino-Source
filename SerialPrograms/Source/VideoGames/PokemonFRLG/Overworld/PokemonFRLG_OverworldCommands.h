/*  Overworld Commands
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Command File Runner commands for FRLG overworld routes:
 *      frlg_plan FROM_MAP x y TO_MAP x y [north|east|south|west]
 *      frlg_route FROM_MAP x y TO_MAP x y [north|east|south|west] [start facing] [Max Repel slot]
 *      frlg_max_repel SLOT
 *  Both plan a route on the decompilation maps (with the post-game flags set)
 *  and print it; frlg_route then walks it. frlg_max_repel uses the Max Repel
 *  in that Items pocket slot (from 1) from the overworld.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldCommands_H
#define PokemonAutomation_PokemonFRLG_OverworldCommands_H

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Registers the commands. Called once at startup, from the FRLG panel
//  list; later calls do nothing.
void register_overworld_commands();


}
}
}
#endif
