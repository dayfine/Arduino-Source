/*  BDSP AutoStory - Lab Console
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registers BDSP's screen detectors (and AutoStory commands) with the
 *  Nintendo Switch Lab Console, so they can be probed on a live game.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_LabConsole_H
#define PokemonAutomation_PokemonBDSP_AutoStory_LabConsole_H

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Safe to call more than once.
void register_bdsp_lab_console();


}
}
}
#endif
