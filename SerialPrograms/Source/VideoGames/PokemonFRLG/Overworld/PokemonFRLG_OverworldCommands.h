/*  Overworld Commands
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Command File Runner commands for FRLG overworld routes:
 *      frlg_plan FROM_MAP x y TO_MAP x y [north|east|south|west]
 *      frlg_route FROM_MAP x y TO_MAP x y [north|east|south|west]
 *  Both plan a route on the decompilation maps (with the post-game flags set)
 *  and print it; frlg_route then walks it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldCommands_H
#define PokemonAutomation_PokemonFRLG_OverworldCommands_H

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


void register_overworld_commands();


}
}
}
#endif
