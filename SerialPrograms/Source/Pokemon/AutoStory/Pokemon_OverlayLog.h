/*  AutoStory Overlay Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Shared word-wrapping video-overlay logger for AutoStory programs.
 *
 */

#ifndef PokemonAutomation_Pokemon_OverlayLog_H
#define PokemonAutomation_Pokemon_OverlayLog_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Color.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The video overlay log box is ~35% of the frame wide at 4% font height:
//  about 34 characters per row.
inline constexpr size_t OVERLAY_LOG_DEFAULT_WIDTH = 34;

//  Splits `text` at spaces into lines no longer than `width` characters
//  (continuations indented by two spaces). A single word longer than `width`
//  is kept intact on its own line.
std::vector<std::string> wrap_overlay_log_lines(
    std::string_view text,
    size_t width = OVERLAY_LOG_DEFAULT_WIDTH
);

//  Adds `text` to `overlay`'s log, wrapped into lines that fit the log box.
void add_wrapped_overlay_log(
    VideoOverlay& overlay,
    std::string_view text,
    Color color,
    size_t width = OVERLAY_LOG_DEFAULT_WIDTH
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
