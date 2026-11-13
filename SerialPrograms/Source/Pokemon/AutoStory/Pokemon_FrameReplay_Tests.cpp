/*  Frame Replay Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The frame source and the replay loop, on synthetic frames: a door's
 *  black between two overworld frames, watched by the shared
 *  BlackScreenOverWatcher the walks use and by the shared fade test.
 *
 */

#include <format>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "Pokemon/Navigation/Pokemon_LegWalker.h"
#include "Pokemon_FrameReplay_Tests.h"
#include "Pokemon_FrameReplay.h"
#include "Pokemon_Interrupts.h"
#include "Pokemon_ScreenEvents.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

using namespace std::chrono_literals;

const WallClock T0 = WallClock() + std::chrono::hours(1);

//  A textured frame: a fine checkerboard, never a fade.
std::shared_ptr<const ImageRGB32> overworld_frame(){
    auto image = std::make_shared<ImageRGB32>(64, 36);
    for (size_t y = 0; y < image->height(); y++){
        for (size_t x = 0; x < image->width(); x++){
            image->pixel(x, y) = (x + y) % 2 == 0 ? 0xff306030 : 0xffd0e0a0;
        }
    }
    return image;
}

std::shared_ptr<const ImageRGB32> black_frame(){
    auto image = std::make_shared<ImageRGB32>(64, 36);
    image->fill(0xff000000);
    return image;
}

//  A door: the overworld, black from 1000 ms to 1500 ms, the overworld again.
ReplayFrameSource door_recording(){
    return ReplayFrameSource({
        {.at = 0ms, .image = overworld_frame()},
        {.at = 1000ms, .image = black_frame()},
        {.at = 1500ms, .image = overworld_frame()},
    });
}

//  Only the shared fade test, as every game's observer reports it.
class FadeObserver : public ScreenEventObserver{
public:
    virtual ScreenEvents observe(const VideoSnapshot& frame, WallClock) override{
        if (!frame){
            return {};
        }
        return fade_events(fade_of_screen(*frame.frame));
    }
    virtual void make_overlays(VideoOverlaySet&) const override{}
};


UnitTestResult test_frame_at(){
    const ReplayFrameSource source = door_recording();
    if (source.size() != 3 || source.last_frame() != 1500ms){
        return "the recording does not hold its three frames";
    }
    const VideoSnapshot black = source.frame_at(1499ms, T0);
    if (!black || fade_of_screen(*black.frame) != WarpFade::BLACK || black.timestamp != T0 + 1499ms){
        return "the frame at 1499 ms is not the black one, stamped 1499 ms";
    }
    if (fade_of_screen(*source.frame_at(1500ms, T0).frame) != WarpFade::NONE){
        return "the frame at 1500 ms is not the overworld";
    }
    const ReplayFrameSource late({{.at = 200ms, .image = black_frame()}});
    if (late.frame_at(100ms, T0)){
        return "a frame shows before the first one";
    }
    return true;
}

UnitTestResult test_missing_folder(){
    try{
        ReplayFrameSource::from_folder("FrameReplay/no such recording/");
    }catch (FileException&){
        return true;
    }
    return "a missing folder gave a recording";
}

//  The walks' wait for a warp (BlackScreenOverWatcher after the hold) sees
//  the door when it watches from before the black.
UnitTestResult test_watcher_sees_fade(){
    BlackScreenOverWatcher arrival;
    const ReplayHit hit = replay_until(door_recording(), {0ms, 8000ms}, {&arrival}, T0);
    if (hit.index != 0 || hit.at < 1500ms || hit.at > 1700ms){
        return std::format("expected the watcher to stop 1500 to 1700 ms in, got index {} at {} ms", hit.index, hit.at.count());
    }
    return true;
}

//  A walk that holds onto a door for 2000 ms, past the black: its watcher
//  sees the hold's frames (WatchAlong) and so the black, over before the
//  hold ends. A watcher started once the black is over never fires: the
//  walk then reported "no fade" though the warp happened (00.27, console
//  B, 2026-11-09 01:41, the warp to CeruleanCity, watched from 1.3 s into
//  the hold).
UnitTestResult test_watcher_along_hold_sees_fade(){
    const ReplayFrameSource source = door_recording();
    BlackScreenOverWatcher arrival;
    WatchAlong along(arrival);
    const ReplayHit during = replay_until(source, {0ms, 2000ms}, {&along}, T0);
    if (during.index != -1 || !along.fired()){
        return std::format("expected the hold not stopped and the fade seen, got index {}, seen {}", during.index, along.fired());
    }
    BlackScreenOverWatcher late;
    const ReplayHit missed = replay_until(source, {2000ms, 10000ms}, {&late}, T0);
    if (missed.index != -1){
        return std::format("expected a watcher started after the black to miss it, got index {} at {} ms", missed.index, missed.at.count());
    }
    return true;
}

UnitTestResult test_events_judged(){
    const ReplayFrameSource source = door_recording();
    FadeObserver observer;
    const ScreenEventLog log = replay_events(source, {0ms, 3000ms}, observer, T0);
    const InterruptWindow whole{T0, T0 + 3000ms};
    const WarpJudgement seen = judge_warp(log, whole, InterruptKnobs{}, SceneChange::DIFFERENT).judgement;
    if (seen != WarpJudgement::WARPED){
        return std::format("from the start: expected warped, got {}", warp_judgement_name(seen));
    }
    FadeObserver late_observer;
    const ScreenEventLog late = replay_events(source, {1600ms, 3000ms}, late_observer, T0);
    const WarpJudgement missed = judge_warp(late, whole, InterruptKnobs{}, SceneChange::DIFFERENT).judgement;
    if (missed != WarpJudgement::NO_FADE){
        return std::format("after the black: expected no fade, got {}", warp_judgement_name(missed));
    }
    return true;
}


struct FrameReplayInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

//  The body checks its own expectations.
struct FrameReplayExpectation{
};

struct FrameReplayCase{
    FrameReplayInput input;
    FrameReplayExpectation expectation;
};

class Test_FrameReplay : public UnitTest{
public:
    explicit Test_FrameReplay(FrameReplayCase test_case)
        : UnitTest("Pokemon::FrameReplay - " + test_case.input.name)
        , m_input(std::move(test_case.input))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_input.body();
    }

private:
    FrameReplayInput m_input;
};

const FrameReplayCase FRAME_REPLAY_CASES[] = {
    {.input = {"the frame showing at a time", test_frame_at}, .expectation = {}},
    {.input = {"a missing recording throws", test_missing_folder}, .expectation = {}},
    {.input = {"the warp watcher sees a door's black", test_watcher_sees_fade}, .expectation = {}},
    {.input = {"a warp watcher along the hold sees a black over before it ends", test_watcher_along_hold_sees_fade}, .expectation = {}},
    {.input = {"the replayed events judged as a warp", test_events_judged}, .expectation = {}},
};

} // namespace


void add_tests_FrameReplay(UnitTestDatabase& database){
    for (const auto& item : FRAME_REPLAY_CASES){
        database.add<Test_FrameReplay>(item);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
