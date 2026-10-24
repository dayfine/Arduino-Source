/*  Game Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_GameOptions_H
#define PokemonAutomation_PokemonSV_GameOptions_H

#include "CommonFramework/Language.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
    struct ProgramInfo;
namespace NintendoSwitch{
namespace PokemonSV{


//  From the overworld, set the game options programs that move by timed inputs need
//  (text speed, move learning, boxes, nicknames, camera controls, Camera Support off,
//  camera interpolation, autosave), save them and return to the overworld. Options
//  the programs don't depend on, such as volume, are left as they are.
//  The main menu must have its full list (Options is the 5th item on the right).
void set_game_options_for_programs(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    Language language
);


}
}
}
#endif
