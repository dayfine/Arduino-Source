/*  Command File Commands
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's detectors and commands for the Nintendo Switch Command File Runner,
 *  so a lab operator can probe BDSP screens and reload the save from a
 *  command file. README.md in the Command File Runner's folder lists them.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_CommandFileCommands_H
#define PokemonAutomation_PokemonBDSP_CommandFileCommands_H

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Registers BDSP's command file detectors and commands. Called once at
//  startup, from the BDSP panel list; later calls do nothing.
void register_command_file_commands();


}
}
}
#endif
