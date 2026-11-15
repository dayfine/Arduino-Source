/*  Covered Watch Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  CoveredWatcher on scripted frames: each case is the capture times of
 *  the frames the pivot hands over (ms after the watch opened, repeats
 *  included), when the wrapped watcher fires and where the watch's frame
 *  deadline is, and on which frame the wrapper asks the session to stop.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Pokemon_CoveredWatch.h"
#include "Pokemon_CoveredWatch_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


const WallClock WATCH_START = WallClock() + Milliseconds(1000000);

WallClock at(int64_t ms){
    return WATCH_START + Milliseconds(ms);
}


//  Fires on every frame captured at or after `fires_at`, and counts the
//  frames handed to it.
class ScriptedWatcher : public VisualInferenceCallback{
public:
    explicit ScriptedWatcher(std::optional<WallClock> fires_at)
        : VisualInferenceCallback("ScriptedWatcher")
        , m_fires_at(fires_at)
    {}
    virtual void make_overlays(VideoOverlaySet&) const override{}
    using VisualInferenceCallback::process_frame;
    virtual bool process_frame(const VideoSnapshot& frame) override{
        m_calls++;
        return m_fires_at && frame.timestamp >= *m_fires_at;
    }
    size_t calls() const{
        return m_calls;
    }
private:
    std::optional<WallClock> m_fires_at;
    size_t m_calls = 0;
};


struct WatchCase{
    struct Input{
        std::vector<int64_t> frames;
        std::optional<int64_t> fires_at = std::nullopt;
        std::optional<int64_t> stop_at = std::nullopt;
    };
    struct Expectation{
        //  The index of the frame the wrapper stops on; none: it never stops.
        std::optional<size_t> stops_on;
        bool fired;
        //  Distinct frames recorded, up to and including the one it stops on.
        size_t frames;
        //  Frames handed to the wrapped watcher, repeats included.
        size_t watcher_calls;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

const std::vector<WatchCase> WATCH_CASES = {
    {.name = "the watcher fires",
     .input = {.frames = {0, 33, 66, 99}, .fires_at = 66},
     .expectation = {.stops_on = 2, .fired = true, .frames = 3, .watcher_calls = 3}},
    {.name = "a frame at the deadline stops the watch without firing",
     .input = {.frames = {0, 33, 66, 99}, .stop_at = 50},
     .expectation = {.stops_on = 2, .fired = false, .frames = 3, .watcher_calls = 3}},
    {.name = "the watcher firing on the deadline frame counts as fired",
     .input = {.frames = {0, 33, 66}, .fires_at = 60, .stop_at = 50},
     .expectation = {.stops_on = 2, .fired = true, .frames = 3, .watcher_calls = 3}},
    {.name = "no deadline and no fire never stops",
     .input = {.frames = {0, 33, 66}},
     .expectation = {.stops_on = std::nullopt, .fired = false, .frames = 3, .watcher_calls = 3}},
    {.name = "old frames handed over again do not reach the deadline",
     .input = {.frames = {0, 10, 10, 10}, .stop_at = 40},
     .expectation = {.stops_on = std::nullopt, .fired = false, .frames = 2, .watcher_calls = 4}},
};


std::string check_watch(const WatchCase& test){
    ScriptedWatcher watcher(test.input.fires_at ? std::optional<WallClock>(at(*test.input.fires_at)) : std::nullopt);
    CoveredWatcher covered(watcher, WATCH_START);
    if (test.input.stop_at){
        covered.stop_at(at(*test.input.stop_at));
    }
    std::optional<size_t> stopped;
    for (size_t c = 0; c < test.input.frames.size(); c++){
        if (covered.process_frame(VideoSnapshot(ImageRGB32(), at(test.input.frames[c])))){
            stopped = c;
            break;
        }
    }
    const WatchCase::Expectation& expected = test.expectation;
    if (stopped != expected.stops_on){
        return std::format(
            "stops on frame: expected {}, got {}",
            expected.stops_on ? std::to_string(*expected.stops_on) : "none",
            stopped ? std::to_string(*stopped) : "none"
        );
    }
    if (covered.fired() != expected.fired){
        return std::format("fired: expected {}, got {}", expected.fired, covered.fired());
    }
    if (covered.coverage().frames() != expected.frames){
        return std::format("frames: expected {}, got {}", expected.frames, covered.coverage().frames());
    }
    if (watcher.calls() != expected.watcher_calls){
        return std::format("watcher calls: expected {}, got {}", expected.watcher_calls, watcher.calls());
    }
    return "";
}


class CoveredWatchTest : public UnitTest{
public:
    explicit CoveredWatchTest(WatchCase test)
        : UnitTest(std::format("Pokemon::CoveredWatch - {}", test.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_watch(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    WatchCase m_case;
};


} // namespace


void add_tests_CoveredWatch(UnitTestDatabase& database){
    for (const WatchCase& test : WATCH_CASES){
        database.add<CoveredWatchTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
