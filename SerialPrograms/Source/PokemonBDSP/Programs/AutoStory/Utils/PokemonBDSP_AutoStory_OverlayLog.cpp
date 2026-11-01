/*  BDSP AutoStory - Overlay Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_OverlayLog.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


void add_wrapped_log(VideoOverlay& overlay, const std::string& text, Color color){
    //  The log box is 35% of the video wide at a font 4% of its height: about
    //  34 characters.
    constexpr size_t WIDTH = 34;
    std::string line;
    size_t start = 0;
    while (start < text.size()){
        size_t end = text.find(' ', start);
        if (end == std::string::npos){
            end = text.size();
        }
        const std::string word = text.substr(start, end - start);
        if (!line.empty() && line.size() + 1 + word.size() > WIDTH){
            overlay.add_log(line, color);
            line = "  ";
        }
        line += (line.empty() || line == "  " ? "" : " ") + word;
        start = end + 1;
    }
    if (!line.empty()){
        overlay.add_log(line, color);
    }
}



}
}
}
