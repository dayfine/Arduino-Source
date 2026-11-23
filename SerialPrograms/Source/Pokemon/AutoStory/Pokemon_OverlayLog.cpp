/*  AutoStory Overlay Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_OverlayLog.h"

namespace PokemonAutomation{
namespace Pokemon{


std::vector<std::string> wrap_overlay_log_lines(std::string_view text, size_t width){
    std::vector<std::string> lines;
    std::string line;
    size_t start = 0;
    while (start < text.size()){
        size_t end = text.find(' ', start);
        if (end == std::string_view::npos){
            end = text.size();
        }
        const std::string_view word = text.substr(start, end - start);
        if (!line.empty() && line.size() + 1 + word.size() > width){
            lines.push_back(std::move(line));
            line = "  ";
        }
        if (!line.empty() && line != "  "){
            line += ' ';
        }
        line.append(word);
        start = end + 1;
    }
    if (!line.empty()){
        lines.push_back(std::move(line));
    }
    return lines;
}

void add_wrapped_overlay_log(
    VideoOverlay& overlay,
    std::string_view text,
    Color color,
    size_t width
){
    for (const std::string& line : wrap_overlay_log_lines(text, width)){
        overlay.add_log(line, color);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
