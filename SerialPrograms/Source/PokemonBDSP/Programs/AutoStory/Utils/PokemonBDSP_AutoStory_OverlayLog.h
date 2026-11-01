/*  BDSP AutoStory - Overlay Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_OverlayLog_H
#define PokemonAutomation_PokemonBDSP_AutoStory_OverlayLog_H

#include <string>
#include "Common/Cpp/Color.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Adds `text` to the video overlay's log, split at spaces into lines short
//  enough for the log box, which draws each line on one row and cuts it off.
void add_wrapped_log(VideoOverlay& overlay, const std::string& text, Color color);


}
}
}
#endif
