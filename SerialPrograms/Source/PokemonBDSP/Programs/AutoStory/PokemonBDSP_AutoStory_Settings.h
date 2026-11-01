/*  BDSP AutoStory - Settings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Settings_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Settings_H

#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  From the overworld: opens Options and sets what the story needs (fast
//  text, Send to Boxes automatic, no nickname prompt, Autosave off), saves
//  them and returns to the overworld. Tries four times, backing out to the
//  overworld between tries, and fails with a screenshot before any movement.
void verify_tutorial_settings(VideoStream& stream, ProControllerContext& context);



}
}
}
#endif
