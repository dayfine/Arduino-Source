/*  FRLG AutoStory - Frame Replay Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Console failures of the walk's warp check, replayed from their frames
 *  through the watcher the walk uses (BlackScreenOverWatcher, from the
 *  start of the hold) and through FRLG's screen events, judged by the
 *  shared judges.
 *  Each case's frames are in CommandLineTests/PokemonFRLG/FrameReplay/<case>/
 *  ("<ms>.png", Pokemon_FrameReplay.h); a case skips when they are missing.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "Pokemon/AutoStory/Pokemon_FrameReplay.h"
#include "Pokemon/AutoStory/Pokemon_Interrupts.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementMechanics.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryFrameReplay_Tests.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

namespace{

using namespace std::chrono_literals;

const WallClock T0 = WallClock() + std::chrono::hours(1);

//  As the walk's WARP_TIMEOUT (PokemonFRLG_OverworldWalk.cpp).
constexpr Milliseconds WARP_WAIT = 8000ms;

//  A one-tile walk onto a door: a tile's time, then the release.
constexpr Milliseconds ONE_TILE_HOLD = MovementMechanics::TILE_TIME + MovementMechanics::STEP_RELEASE;

//  A warp leg: the player holds onto the door for `hold`, watched for the
//  fade (warp_hold_watchers()), and the walk then waits out the rest of it
//  (wait_out_warp()).
struct WarpReplayInput{
    std::string name;
    //  The case's folder under CommandLineTests/PokemonFRLG/FrameReplay/.
    std::string recording;
    Milliseconds hold;
};

struct WarpReplayExpectation{
    //  "fade over during the hold", "fade over at <ms> ms" or "no fade",
    //  from BlackScreenOverWatcher.
    std::string watcher;
    //  judge_warp() and classify_interrupt() over FRLG's events.
    std::string warp;
    std::string interrupt;
};

struct WarpReplayCase{
    WarpReplayInput input;
    WarpReplayExpectation expectation;
};

std::string replay_watcher(const Pokemon::ReplayFrameSource& source, const WarpReplayInput& input){
    BlackScreenOverWatcher arrival(COLOR_RED);
    Pokemon::WatchAlong fade(arrival);
    Pokemon::replay_until(source, {Milliseconds(0), input.hold}, {&fade}, T0);
    if (fade.fired()){
        return "fade over during the hold";
    }
    const Pokemon::ReplayWindow wait{input.hold, input.hold + WARP_WAIT};
    const Pokemon::ReplayHit hit = Pokemon::replay_until(source, wait, {&arrival}, T0);
    return hit.index < 0 ? "no fade" : std::format("fade over at {} ms", hit.at.count());
}

class Test_WarpReplay : public UnitTest{
public:
    explicit Test_WarpReplay(WarpReplayCase test_case)
        : UnitTest("PokemonFRLG::AutoStory - Frame replay: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string folder = UNIT_TEST_RESOURCE_PATH() + "PokemonFRLG/FrameReplay/" + m_case.input.recording + "/";
        std::optional<Pokemon::ReplayFrameSource> source;
        try{
            source.emplace(Pokemon::ReplayFrameSource::from_folder(folder));
        }catch (FileException& e){
            return UnitTestResult(UnitTestResult::SKIPPED, e.message());
        }
        const std::string watcher = replay_watcher(*source, m_case.input);

        FrlgScreenObserver observer;
        const Pokemon::ReplayWindow whole{0ms, m_case.input.hold + WARP_WAIT};
        const Pokemon::ScreenEventLog log = Pokemon::replay_events(*source, whole, observer, T0);
        const Pokemon::InterruptWindow window{T0, T0 + whole.end};
        const std::string warp = Pokemon::warp_judgement_name(
            Pokemon::judge_warp(log, window, Pokemon::InterruptKnobs{}, Pokemon::SceneChange::UNKNOWN).judgement
        );
        const std::string interrupt = Pokemon::interrupt_kind_name(
            Pokemon::classify_interrupt(log, window, Pokemon::InterruptKnobs{}).kind
        );

        const WarpReplayExpectation& expected = m_case.expectation;
        if (watcher != expected.watcher || warp != expected.warp || interrupt != expected.interrupt){
            return std::format(
                "expected watcher \"{}\", warp \"{}\", interrupt \"{}\"; got \"{}\", \"{}\", \"{}\"; events:\n{}",
                expected.watcher, expected.warp, expected.interrupt, watcher, warp, interrupt,
                Pokemon::describe(log, T0)
            );
        }
        return true;
    }

private:
    WarpReplayCase m_case;
};

//  00.27 route10_nurse, console B 2026-11-09, runs 3 and 2: "no fade in
//  and out after walking onto the warp to Route10_PokemonCenter_1F". The
//  frames are the end screens of the runs (no stream history was kept):
//  the player on Route10 (14, 10) in the grass and on (16, 19) beside the
//  Center, not on its door (13, 20). The detectors agree there was no
//  fade: the walk ended off the door (timed legs one tile off), not a
//  missed fade (FRLGLab notes 2026-11-09-0027-route10-evidence).
const WarpReplayCase WARP_REPLAY_CASES[] = {
    {
        .input = {"00.27 route10_nurse run 3: ended in the grass, no fade", "0027-route10-run3", ONE_TILE_HOLD},
        .expectation = {"no fade", "no fade", "none"},
    },
    {
        .input = {"00.27 route10_nurse run 2: ended beside the Center, no fade", "0027-route10-run2", ONE_TILE_HOLD},
        .expectation = {"no fade", "no fade", "none"},
    },
};

} // namespace


void add_tests_AutoStoryFrameReplay(UnitTestDatabase& database){
    for (const auto& item : WARP_REPLAY_CASES){
        database.add<Test_WarpReplay>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
