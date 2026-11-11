/*  AutoStory Story Preflight
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <chrono>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTools/FloatPixel.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "Pokemon_StoryPreflight.h"

namespace PokemonAutomation{
namespace Pokemon{

using namespace std::chrono_literals;


namespace{

//  Color bars are seven flat vertical stripes of different colors.
constexpr size_t BAR_COUNT = 7;
//  A stripe flatter than this (stddev sum) is one solid color.
constexpr double BAR_MAX_STDDEV = 30;
//  Two stripes this far apart (RGB distance) are different bars.
constexpr double BAR_MIN_DISTANCE = 60;
//  A frame older than this is a stalled feed.
constexpr auto FRAME_MAX_AGE = 2000ms;

bool shows_color_bars(const ImageViewRGB32& frame){
    std::array<FloatPixel, BAR_COUNT> bars;
    for (size_t c = 0; c < BAR_COUNT; c++){
        const double width = 1.0 / BAR_COUNT;
        const ImageFloatBox stripe(c * width + width * 0.25, 0.1, width * 0.5, 0.5);
        const ImageStats stats = image_stats(extract_box_reference(frame, stripe));
        if (stats.stddev.sum() > BAR_MAX_STDDEV){
            return false;
        }
        bars[c] = stats.average;
    }
    for (size_t c = 1; c < BAR_COUNT; c++){
        if (euclidean_distance(bars[c - 1], bars[c]) < BAR_MIN_DISTANCE){
            return false;
        }
    }
    return true;
}

bool frame_shows_picture(const ImageViewRGB32& frame){
    if (!frame){
        return false;
    }
    if (is_black(frame)){
        return false;
    }
    return !shows_color_bars(frame);
}

} // namespace

StoryPreflight StoryPreflight::read(NintendoSwitch::ConsoleHandle& console, NintendoSwitch::ProControllerContext& context){
    StoryPreflight preflight;
    preflight.controller_ready = context.controller().is_ready();
    const VideoSnapshot snapshot = console.video().snapshot();
    const bool fresh = snapshot && current_time() - snapshot.timestamp < FRAME_MAX_AGE;
    preflight.video_alive = fresh && frame_shows_picture(snapshot);
    preflight.game_running = preflight.video_alive
        && !NintendoSwitch::HomeMenuDetector(console, COLOR_RED).detect(snapshot);
    return preflight;
}


Outcome StoryPreflight::check() const{
    std::string failed;
    auto add = [&failed](bool ok, std::string_view what){
        if (ok){
            return;
        }
        if (!failed.empty()){
            failed += "; ";
        }
        failed += what;
    };
    add(video_alive, "no live video");
    add(controller_ready, "the controller is not ready");
    add(game_running, "the game is not running");
    if (failed.empty()){
        return outcome_ok();
    }
    return outcome_failed(std::string(FAILURE), failed);
}



} // namespace Pokemon
} // namespace PokemonAutomation
