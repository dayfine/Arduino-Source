/*  Frame Coverage Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A watch window judged on scripted frame times: each case is the capture
 *  times of the frames a watcher inspected (ms after the window opened, in
 *  the order inspected), whether it fired, and where the window ends.
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_FrameCoverage.h"
#include "Pokemon_FrameCoverage_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


const WallClock WINDOW_START = WallClock() + Milliseconds(1000000);

WallClock at(int64_t ms){
    return WINDOW_START + Milliseconds(ms);
}

//  `count` frames `period` ms apart from `first`.
std::vector<int64_t> frames_every(int64_t first, int64_t period, size_t count){
    std::vector<int64_t> out;
    for (size_t c = 0; c < count; c++){
        out.push_back(first + period * (int64_t)c);
    }
    return out;
}

//  `a` then `b`.
std::vector<int64_t> joined(std::vector<int64_t> a, const std::vector<int64_t>& b){
    a.insert(a.end(), b.begin(), b.end());
    return a;
}


struct CoverageCase{
    struct Input{
        std::vector<int64_t> frames;
        bool fired = false;
        int64_t window_end = 8000;
        Milliseconds max_gap = Milliseconds(100);
    };
    struct Expectation{
        WatchVerdict verdict;
        size_t frames;
        Milliseconds largest_gap;
        //  Whether a frame captured at or after the window's end was inspected.
        bool reached_end;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

const std::vector<CoverageCase> COVERAGE_CASES = {
    {.name = "frames every 33 ms, watcher fired",
     .input = {.frames = frames_every(10, 33, 100), .fired = true, .window_end = 3000},
     .expectation = {.verdict = WatchVerdict::SEEN, .frames = 100, .largest_gap = Milliseconds(33), .reached_end = true}},
    {.name = "frames every 33 ms through the window, not fired",
     .input = {.frames = frames_every(10, 33, 250)},
     .expectation = {.verdict = WatchVerdict::NOT_SEEN, .frames = 250, .largest_gap = Milliseconds(33), .reached_end = true}},
    {.name = "a second of dropped frames mid-window",
     .input = {.frames = joined(frames_every(10, 33, 100), frames_every(4300, 33, 120))},
     .expectation = {.verdict = WatchVerdict::UNKNOWN, .frames = 220, .largest_gap = Milliseconds(1023), .reached_end = true}},
    {.name = "no frame inspected",
     .input = {.frames = {}},
     .expectation = {.verdict = WatchVerdict::UNKNOWN, .frames = 0, .largest_gap = Milliseconds(8000), .reached_end = false}},
    {.name = "frames stop before the window ends",
     .input = {.frames = frames_every(10, 33, 200)},
     .expectation = {.verdict = WatchVerdict::UNKNOWN, .frames = 200, .largest_gap = Milliseconds(1423), .reached_end = false}},
    {.name = "first frame late after the window opened",
     .input = {.frames = frames_every(500, 33, 240)},
     .expectation = {.verdict = WatchVerdict::UNKNOWN, .frames = 240, .largest_gap = Milliseconds(500), .reached_end = true}},
    {.name = "the same frame handed over again counts once",
     .input = {.frames = {10, 10, 10, 43, 43, 76}, .window_end = 80},
     .expectation = {.verdict = WatchVerdict::NOT_SEEN, .frames = 3, .largest_gap = Milliseconds(33), .reached_end = false}},
    {.name = "a frame captured before the window opened is not evidence",
     .input = {.frames = {-40, 60, 93}, .window_end = 90},
     .expectation = {.verdict = WatchVerdict::NOT_SEEN, .frames = 2, .largest_gap = Milliseconds(60), .reached_end = true}},
    {.name = "fired after dropped frames is still seen",
     .input = {.frames = {10, 1500, 1533}, .fired = true, .window_end = 2000},
     .expectation = {.verdict = WatchVerdict::SEEN, .frames = 3, .largest_gap = Milliseconds(1490), .reached_end = false}},
    {.name = "gaps of exactly the max gap still cover",
     .input = {.frames = frames_every(0, 100, 11), .window_end = 1000},
     .expectation = {.verdict = WatchVerdict::NOT_SEEN, .frames = 11, .largest_gap = Milliseconds(100), .reached_end = true}},
};


std::string check_coverage(const CoverageCase& test){
    FrameCoverage coverage(WINDOW_START);
    for (int64_t ms : test.input.frames){
        coverage.record(at(ms));
    }
    const WallClock end = at(test.input.window_end);
    const WatchResult result = judge_watch(test.input.fired, coverage, end, test.input.max_gap);
    const CoverageCase::Expectation& expected = test.expectation;
    if (result.verdict != expected.verdict){
        return std::format("verdict: expected \"{}\", got \"{}\"", describe(expected.verdict), describe(result));
    }
    if (result.frames != expected.frames){
        return std::format("frames: expected {}, got {}", expected.frames, result.frames);
    }
    if (result.largest_gap != expected.largest_gap){
        return std::format("largest gap: expected {} ms, got {} ms", expected.largest_gap.count(), result.largest_gap.count());
    }
    if (coverage.reached(end) != expected.reached_end){
        return std::format("reached the window's end: expected {}, got {}", expected.reached_end, !expected.reached_end);
    }
    return "";
}


class FrameCoverageTest : public UnitTest{
public:
    explicit FrameCoverageTest(CoverageCase test)
        : UnitTest(std::format("Pokemon::FrameCoverage - {}", test.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_coverage(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    CoverageCase m_case;
};


} // namespace


void add_tests_FrameCoverage(UnitTestDatabase& database){
    for (const CoverageCase& test : COVERAGE_CASES){
        database.add<FrameCoverageTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
