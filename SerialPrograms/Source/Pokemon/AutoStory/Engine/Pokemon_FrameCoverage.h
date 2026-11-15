/*  Frame Coverage
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which frames a watch over a window actually inspected, and whether they
 *  are enough to say a screen did not show. The inference pivot polls a
 *  watcher every period with the newest converted frame, and that frame
 *  can be the one it saw last time or one captured a second ago: under CPU
 *  load (ConvertFrame at 150 ms mean, PLZA 03.08 on 2026-11-14) a watcher
 *  that polled 160 times over 8 s may have seen a few frames with second-long
 *  holes between them, and a 2 s "Registered to the Pokedex" banner fell
 *  into one. So a window is judged on the capture times of the frames
 *  inspected (VideoSnapshot::timestamp), not on polls or wall time.
 *
 */

#ifndef PokemonAutomation_Pokemon_FrameCoverage_H
#define PokemonAutomation_Pokemon_FrameCoverage_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The distinct frames inspected since a window opened, by capture time.
class FrameCoverage{
public:
    explicit FrameCoverage(WallClock start);

    //  A frame captured at `frame` was inspected. A frame captured before
    //  the window opened, or not after the last one recorded (the pivot
    //  handing over the same frame again), adds nothing.
    void record(WallClock frame);

    //  When the window opened.
    WallClock start() const{
        return m_start;
    }
    //  How many distinct frames were recorded.
    size_t frames() const{
        return m_frames.size();
    }
    //  The capture time of the newest frame; none before the first.
    std::optional<WallClock> last_frame() const;
    //  Whether a frame captured at or after `deadline` was inspected: the
    //  window up to `deadline` is over in frame time, not only in wall time.
    bool reached(WallClock deadline) const;
    //  The longest part of [start, `end`] no inspected frame was captured
    //  in: before the first frame, between two, or after the last. Rounded
    //  up to whole ms, so a gap a fraction over a limit is over it.
    Milliseconds largest_gap(WallClock end) const;

private:
    WallClock m_start;
    std::vector<WallClock> m_frames;
};


//  What a watch window showed.
enum class WatchVerdict{
    SEEN,       //  The watcher fired.
    NOT_SEEN,   //  It did not, and the frames cover the window.
    UNKNOWN,    //  It did not, but a gap between frames could hide the screen.
};

//  A watch window's verdict and the frames it rests on.
struct WatchResult{
    WatchVerdict verdict = WatchVerdict::UNKNOWN;
    size_t frames = 0;
    Milliseconds largest_gap = Milliseconds(0);
};

//  SEEN when the watcher fired. Otherwise NOT_SEEN only when no gap in
//  [coverage's start, `end`] is longer than `max_gap`, else UNKNOWN: the
//  caller looks again or falls back instead of reading "not there". A
//  watcher that must see the screen for a hold (DetectorToFinder) needs
//  frames at least that often, so `max_gap` is at most the hold, and at
//  most how long the screen shows minus the hold.
WatchResult judge_watch(bool fired, const FrameCoverage& coverage, WallClock end, Milliseconds max_gap);

//  "seen", "not seen", "unknown".
std::string describe(WatchVerdict verdict);
//  "not seen over 240 frames, largest gap 0.03 s",
//  "unknown: largest gap between frames 1.02 s over 220 frames".
std::string describe(const WatchResult& result);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
