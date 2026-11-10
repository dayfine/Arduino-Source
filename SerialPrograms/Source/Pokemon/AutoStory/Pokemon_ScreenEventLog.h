/*  Screen Event Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Split out of Pokemon_ScreenEvents.h, unchanged; it includes this header.
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenEventLog_H
#define PokemonAutomation_Pokemon_ScreenEventLog_H

#include <atomic>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <optional>
#include <string>
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "Pokemon_ScreenFades.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Whether an event started or stopped showing.
enum class ScreenEventEdge : uint8_t{
    ROSE,
    FELL,
};

//  One event starting or stopping to show, at the frame that showed it.
struct ScreenEventChange{
    WallClock time;
    ScreenEvent event;
    ScreenEventEdge edge;

    bool operator==(const ScreenEventChange& other) const = default;
};

//  How many changes a log keeps. A walk's events change a few times a
//  second at most (a fade's black, a box, the screen going still), so this
//  is a minute or more of a busy scene.
constexpr size_t SCREEN_EVENT_LOG_CAPACITY = 256;

//  The events of a stream of frames, in order and timed: what rose and fell
//  when, not only what was seen (ScreenEventWatcher::seen()). The judges that
//  care about order read it (Pokemon_Interrupts.h): a flash then a battle is
//  an encounter, a box then a battle a trainer's sight.
//
//  It keeps the newest `capacity` changes and counts the ones it dropped.
//  Not thread-safe; ScreenEventWatcher guards its own.
class ScreenEventLog{
public:
    explicit ScreenEventLog(size_t capacity = SCREEN_EVENT_LOG_CAPACITY);

    //  Adds a frame: a change for each event that rose or fell since the
    //  frame before (the first frame's events all rise). `now` must not go
    //  back in time.
    void record(const ScreenEvents& frame, WallClock now);

    //  The changes kept, oldest first.
    const std::deque<ScreenEventChange>& changes() const{
        return m_changes;
    }
    //  How many changes were dropped, oldest first, to keep the capacity.
    size_t dropped() const{
        return m_dropped;
    }
    //  The events of the last frame recorded.
    ScreenEvents showing() const{
        return m_showing;
    }
    //  When the last frame was recorded; none before the first.
    std::optional<WallClock> last_frame() const{
        return m_last_frame;
    }
    void clear();

private:
    size_t m_capacity;
    std::deque<ScreenEventChange> m_changes;
    size_t m_dropped = 0;
    ScreenEvents m_showing;
    std::optional<WallClock> m_last_frame;
};
//  "+1200 ms dialog box rose", times from `origin`, one change per line.
std::string describe(const ScreenEventLog& log, WallClock origin);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
