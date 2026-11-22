/*  BDSP Blink Extraction
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_BlinkExtraction_H
#define PokemonAutomation_PokemonBDSP_BlinkExtraction_H

#include <stddef.h>
#include <optional>
#include <string>
#include <vector>
#include "PokemonBDSP/Programs/RngManipulation/PokemonBDSP_BlinkModel.h"
#include "PokemonBDSP/Programs/RngManipulation/PokemonBDSP_StateSolver.h"
#include "PokemonBDSP_EyeBlinkDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


struct Blink{
    double seconds = 0;
    double depth = 0;
    size_t frames = 0;
};

struct BlinkEvent{
    double seconds = 0;
    //  Empty when a capture stall overlapped the +/-double_blink_seconds window
    //  around a Single blink (either dip of a Double may have been dropped), so
    //  single vs. double cannot be trusted and bit 0 is omitted.
    std::optional<BlinkType> type = BlinkType::Single;
};

struct TickFit{
    double period_seconds = 0;
    double rms_ticks = 0;
    double worst_ticks = 0;
};


//  How far the match has to fall below its resting level to count as a blink
double auto_blink_threshold(const std::vector<BlinkMatchSample>& samples);


std::vector<Blink> extract_blinks(
    const std::vector<BlinkMatchSample>& samples, double threshold,
    WallClock origin = WallClock::min()
);

struct BlinkStallGuardConfig{
    static constexpr double DEFAULT_DOUBLE_BLINK_SECONDS = 0.55;
    double double_blink_seconds = DEFAULT_DOUBLE_BLINK_SECONDS;
    double max_frame_gap_seconds = 0.065;
};

//  A second blink arriving within "double_blink_seconds" is the same roll
std::vector<BlinkEvent> group_blinks(
    const std::vector<Blink>& blinks,
    double double_blink_seconds = BlinkStallGuardConfig::DEFAULT_DOUBLE_BLINK_SECONDS
);

//  Clear `event.type` on any Single blink whose symmetric +/-`double_blink_seconds`
//  window overlapped a capture stall (adjacent samples further apart than
//  `max_frame_gap_seconds`), since either dip of a Double blink may have been dropped.
//  Precondition: `samples` and `events` are sorted in ascending time order.
void mark_uncertain_blink_types(
    const std::vector<BlinkMatchSample>& samples,
    WallClock origin,
    const BlinkStallGuardConfig& config,
    std::vector<BlinkEvent>& events
);

//  The NPC tick is nominally 1.017 s but measures 1.0197 s on real hardware
TickFit fit_tick_period(
    const std::vector<BlinkEvent>& events,
    double lowest_seconds = 1.000,
    double highest_seconds = 1.040
);

bool build_samples(
    const std::vector<std::vector<BlinkEvent>>& streams,
    const std::vector<uint8_t>& slots,
    double period_seconds,
    std::vector<BlinkSample>& samples,
    std::string& failure_reason
);

bool last_blink_anchor(
    const std::vector<std::vector<BlinkEvent>>& streams,
    const std::vector<uint8_t>& slots,
    double period_seconds,
    uint64_t& advance,
    double& seconds,
    size_t& stream_index
);


//  Move an anchor forward onto a later blink from the same stream.
bool step_blink_anchor(
    double elapsed_seconds,
    double period_seconds,
    uint8_t npcs,
    uint64_t previous_advance,
    uint64_t& advance
);


}
}
}
#endif
