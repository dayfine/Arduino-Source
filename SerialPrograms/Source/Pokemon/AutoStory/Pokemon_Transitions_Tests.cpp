/*  Transitions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The transition tracker on synthetic frame statistics: a frame is its
 *  middle's mean and stddev at a time. Then the same tracker on captured
 *  frames from CommandLineTests, when they are checked out.
 *
 */

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Pokemon_Interrupts.h"
#include "Pokemon_Transitions.h"
#include "Pokemon_Transitions_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

using namespace std::chrono_literals;

const WallClock T0 = WallClock() + std::chrono::hours(1);

ImageStats stats(double r, double g, double b, double stddev){
    return ImageStats(FloatPixel(r, g, b), FloatPixel(stddev, stddev, stddev), 1000);
}

//  Scenes are textured; each has its own mean, which the comparison reads.
const ImageStats ROUTE = stats(120, 100, 90, 40);
const ImageStats HOUSE = stats(100, 120, 90, 40);
//  A cave: dark, but textured past the partial tier.
const ImageStats CAVE = stats(50, 45, 40, 25);
//  The Oreburgh Gate's dim floor: mean 46, textured within the partial tier.
const ImageStats DIM_FLOOR = stats(46, 46, 46, 8);
//  A street at night: dark, but lit windows keep it textured.
const ImageStats NIGHT = stats(40, 40, 55, 30);
const ImageStats FALLING_DARK = stats(60, 60, 60, 10);
const ImageStats DARK = stats(5, 5, 5, 1);
const ImageStats FALLING_WHITE = stats(160, 160, 160, 10);
const ImageStats WHITE = stats(250, 250, 250, 1);
//  An empty or dropped frame: no stats at all.
const ImageStats EMPTY;

SceneChange compare_means(const TransitionSample& before, const TransitionSample& after){
    const FloatPixel& a = before.middle.average;
    const FloatPixel& b = after.middle.average;
    return a.r == b.r && a.g == b.g && a.b == b.b ? SceneChange::SAME : SceneChange::DIFFERENT;
}

//  `stats` from `from_ms` to `to_ms`, a frame every 100 ms.
struct Span{
    int from_ms;
    int to_ms;
    ImageStats stats;
};

//  The transitions completed, as describe() puts them, joined by "; ";
//  "none" if none.
std::string run_tracker(const std::vector<Span>& spans, TransitionTracker& tracker){
    std::string out;
    for (const Span& span : spans){
        for (int ms = span.from_ms; ms <= span.to_ms; ms += 100){
            const std::optional<Transition> done = tracker.push({T0 + Milliseconds(ms), span.stats, {}});
            if (done){
                out += (out.empty() ? "" : "; ") + describe(*done, T0);
            }
        }
    }
    return out.empty() ? "none" : out;
}


struct TrackCase{
    //  A synthetic stream of frames, through one game's profile.
    struct Input{
        std::string name;
        std::vector<Span> spans;
        const FadeProfile* profile = &BDSP_FADE_PROFILE;
        bool compare = true;
    };

    struct Expectation{
        //  The transitions completed, as run_tracker() puts them.
        std::string transitions;
        //  What judge_warp() makes of the tracker's current() at the end; empty
        //  for none.
        std::string warp = "";
    };

    Input input;
    Expectation expectation;
};

class Test_TransitionTracker : public UnitTest{
public:
    explicit Test_TransitionTracker(TrackCase test_case)
        : UnitTest(std::format("Pokemon::Transitions - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const TrackCase::Input& input = m_case.input;
        TransitionTracker tracker(*input.profile, input.compare ? compare_means : TransitionTracker::CompareScenes());
        const std::string got = run_tracker(input.spans, tracker);
        if (got != m_case.expectation.transitions){
            return std::format("transitions: expected \"{}\", got \"{}\"", m_case.expectation.transitions, got);
        }
        const std::optional<Transition> current = tracker.current();
        const InterruptKnobs knobs{.min_fade_hold = 200ms};
        const std::string warp = current ? warp_judgement_name(judge_warp(*current, knobs).judgement) : "";
        if (warp != m_case.expectation.warp){
            return std::format("warp: expected \"{}\", got \"{}\"", m_case.expectation.warp, warp);
        }
        return true;
    }

private:
    TrackCase m_case;
};


UnitTestResult test_timeout(){
    TransitionTracker tracker(BDSP_FADE_PROFILE, compare_means);
    const std::string got = run_tracker({{0, 100, ROUTE}, {200, 8200, DARK}}, tracker);
    if (got != "none" || tracker.phase() != TransitionPhase::HOLD){
        return "a screen that stays dark completes no transition and stays in its hold";
    }
    if (tracker.timed_out(T0 + 8200ms) || !tracker.timed_out(T0 + 8300ms)){
        return "a transition times out max_hold after it started, not before";
    }
    const std::string current = describe(*tracker.current(), T0);
    if (current != "black fade +200 ms, peak +200 ms, rise unseen, in progress"){
        return std::format("current(): {}", current);
    }
    tracker.reset();
    if (tracker.phase() != TransitionPhase::SCENE || tracker.current() || tracker.timed_out(T0 + 9000ms)){
        return "reset() did not clear the tracker";
    }
    return true;
}

UnitTestResult test_sample_of_frame(){
    ImageRGB32 image(40, 40);
    image.fill(0xff000000);
    const TransitionSample sample = transition_sample(VideoSnapshot(std::move(image), T0));
    TransitionTracker tracker(BDSP_FADE_PROFILE);
    tracker.push(sample);
    if (sample.time != T0 || tracker.phase() != TransitionPhase::HOLD){
        return "a black frame's sample is not a hold at its timestamp";
    }
    return transition_sample(VideoSnapshot()).middle.count == 0
        ? UnitTestResult(true)
        : UnitTestResult("an empty frame has stats");
}


struct CapturedCase{
    //  Captured frames: CommandLineTests/<game>/Transitions/<case>/ holds
    //  frames named by their time in ms ("0.png", "250.png"), and expected.txt
    //  with the transitions run_tracker() should report (no scene comparison).
    struct Input{
        std::string game;
        const FadeProfile* profile;
    };

    //  Each case folder's expected.txt holds its expectation.
    struct Expectation{
    };

    Input input;
    Expectation expectation;
};

std::string track_captured_case(const Filesystem::Path& folder, const FadeProfile& profile){
    std::vector<std::pair<int, Filesystem::Path>> frames;
    for (const Filesystem::Path& file : Filesystem::list_directory(folder)){
        std::string extension = file.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c){
            return static_cast<char>(std::tolower(c));
        });
        if (extension != ".png"){
            continue;
        }
        const std::string stem = file.stem().string();
        int ms = 0;
        const auto parsed = std::from_chars(stem.data(), stem.data() + stem.size(), ms);
        if (parsed.ec != std::errc() || parsed.ptr != stem.data() + stem.size()){
            return std::format("frame {} is not named by its time in ms", file.filename().string());
        }
        frames.emplace_back(ms, file);
    }
    std::sort(frames.begin(), frames.end(), [](const auto& a, const auto& b){
        return a.first < b.first;
    });
    TransitionTracker tracker(profile);
    std::string out;
    for (const auto& frame : frames){
        VideoSnapshot snapshot(ImageRGB32(frame.second.string()), T0 + Milliseconds(frame.first));
        const std::optional<Transition> done = tracker.push(transition_sample(snapshot));
        if (done){
            out += (out.empty() ? "" : "; ") + describe(*done, T0);
        }
    }
    return out.empty() ? "none" : out;
}

class Test_CapturedTransitions : public UnitTest{
public:
    explicit Test_CapturedTransitions(CapturedCase test_case)
        : UnitTest(std::format("Pokemon::Transitions - captured frames: {}", test_case.input.game))
        , m_input(std::move(test_case.input))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const Filesystem::Path root(std::format("{}{}/Transitions/", UNIT_TEST_RESOURCE_PATH(), m_input.game));
        if (!Filesystem::is_directory(root)){
            return UnitTestResult(UnitTestResult::SKIPPED, std::format("no captured transitions at {}", root.string()));
        }
        for (const Filesystem::Path& folder : Filesystem::list_directory(root)){
            if (!Filesystem::is_directory(folder)){
                continue;
            }
            std::string expected = file_to_string((folder / "expected.txt").string());
            expected.erase(expected.find_last_not_of("\r\n") + 1);
            const std::string got = track_captured_case(folder, *m_input.profile);
            if (got != expected){
                return std::format("{}: expected \"{}\", got \"{}\"", folder.filename().string(), expected, got);
            }
        }
        return true;
    }

private:
    CapturedCase::Input m_input;
};


struct TransitionsCase{
    struct Input{
        std::string name;
        UnitTestResult (*body)();
    };

    //  The body checks its own expectations.
    struct Expectation{
    };

    Input input;
    Expectation expectation;
};

class Test_Transitions : public UnitTest{
public:
    explicit Test_Transitions(TransitionsCase test_case)
        : UnitTest(std::format("Pokemon::Transitions - {}", test_case.input.name))
        , m_input(std::move(test_case.input))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_input.body();
    }

private:
    TransitionsCase::Input m_input;
};

} // namespace


const TrackCase TRACK_CASES[] = {
    {
        .input = {"a door: falls, holds, rises onto another scene",
            {{0, 100, ROUTE}, {200, 200, FALLING_DARK}, {300, 700, DARK}, {800, 800, FALLING_DARK}, {900, 1000, HOUSE}}},
        .expectation = {"black fade +200 ms, peak +300 ms, rise +800 ms, end +900 ms, other scene", "warped"},
    },
    {
        .input = {"a missed short fade: falling, then another scene",
            {{0, 100, ROUTE}, {200, 200, FALLING_DARK}, {300, 400, HOUSE}}},
        .expectation = {"black fade +200 ms, peak unseen, rise unseen, end +300 ms, other scene", "warped"},
    },
    {
        .input = {"a missed fade is not reported with no scene comparison",
            {{0, 100, ROUTE}, {200, 200, FALLING_DARK}, {300, 400, HOUSE}}, &BDSP_FADE_PROFILE, false},
        .expectation = {"none"},
    },
    {
        .input = {"a dip back onto the same scene is no transition",
            {{0, 100, ROUTE}, {200, 300, FALLING_DARK}, {400, 500, ROUTE}}},
        .expectation = {"none"},
    },
    {
        .input = {"a one-frame black onto the same scene is too short",
            {{0, 100, ROUTE}, {200, 200, DARK}, {300, 400, ROUTE}}},
        .expectation = {"black fade +200 ms, peak +200 ms, rise +300 ms, end +300 ms, same scene", "same scene after the fade"},
    },
    {
        .input = {"a one-frame black, scene unknown, is too short",
            {{0, 100, ROUTE}, {200, 200, DARK}, {300, 400, ROUTE}}, &BDSP_FADE_PROFILE, false},
        .expectation = {"black fade +200 ms, peak +200 ms, rise +300 ms, end +300 ms, scene unknown", "fade too short"},
    },
    {
        .input = {"an encounter's flash: a short white, back on the scene",
            {{0, 100, ROUTE}, {200, 200, WHITE}, {300, 400, ROUTE}}},
        .expectation = {"flash +200 ms, peak +200 ms, rise +300 ms, end +300 ms, same scene", "an encounter's flash"},
    },
    {
        .input = {"a cave exit: a long white onto another scene",
            {{0, 100, CAVE}, {200, 200, FALLING_WHITE}, {300, 800, WHITE}, {900, 900, FALLING_WHITE}, {1000, 1100, ROUTE}}},
        .expectation = {"white fade +200 ms, peak +300 ms, rise +900 ms, end +1000 ms, other scene", "warped"},
    },
    {
        .input = {"a white between a flash and a fade, scene unknown, is unsure",
            {{0, 100, ROUTE}, {200, 400, WHITE}, {500, 600, ROUTE}}, &BDSP_FADE_PROFILE, false},
        .expectation = {"white, flash or fade +200 ms, peak +200 ms, rise +500 ms, end +500 ms, scene unknown", "a white, flash or fade"},
    },
    {
        .input = {"a short white onto another scene may be the battle: unsure",
            {{0, 100, ROUTE}, {200, 200, WHITE}, {300, 400, HOUSE}}},
        .expectation = {"white, flash or fade +200 ms, peak +200 ms, rise +300 ms, end +300 ms, other scene", "a white, flash or fade"},
    },
    {
        .input = {"FRLG has no white fade: a long white is a flash",
            {{0, 100, CAVE}, {300, 800, WHITE}, {1000, 1100, ROUTE}}, &FRLG_FADE_PROFILE},
        .expectation = {"flash +300 ms, peak +300 ms, rise +1000 ms, end +1000 ms, other scene", "an encounter's flash"},
    },
    {
        .input = {"the Oreburgh Gate's dim floor is no fade, a door from it is",
            {{0, 100, ROUTE}, {200, 3000, DIM_FLOOR}, {3100, 3400, DARK}, {3500, 3600, HOUSE}}},
        .expectation = {"black fade +3100 ms, peak +3100 ms, rise +3500 ms, end +3500 ms, other scene", "warped"},
    },
    {
        .input = {"a street at night is no fade", {{0, 5000, NIGHT}}},
        .expectation = {"none"},
    },
    {
        .input = {"a cave mouth's fade rises into a dim cave",
            {{0, 100, ROUTE}, {200, 500, DARK}, {600, 2500, DIM_FLOOR}}},
        .expectation = {"black fade +200 ms, peak +200 ms, rise +600 ms, end +1700 ms, other scene", "warped"},
    },
    {
        .input = {"empty frames in a scene start no fade",
            {{0, 100, ROUTE}, {200, 300, EMPTY}, {400, 500, ROUTE}}},
        .expectation = {"none"},
    },
    {
        .input = {"an empty frame in a fall is skipped",
            {{0, 100, ROUTE}, {200, 200, EMPTY}, {300, 300, FALLING_DARK}, {400, 600, DARK}}},
        .expectation = {"none", "fade still showing"},
    },
    {
        .input = {"an empty frame in the rise does not move the rise",
            {{0, 100, ROUTE}, {200, 200, FALLING_DARK}, {300, 500, DARK}, {600, 600, FALLING_DARK},
             {700, 700, EMPTY}, {800, 900, HOUSE}}},
        .expectation = {"black fade +200 ms, peak +300 ms, rise +600 ms, end +800 ms, other scene", "warped"},
    },
    {
        .input = {"a fade in progress is still showing",
            {{0, 100, ROUTE}, {200, 200, FALLING_DARK}, {300, 600, DARK}}},
        .expectation = {"none", "fade still showing"},
    },
};

const TransitionsCase TRANSITIONS_CASES[] = {
    {
        .input = {"a screen that stays dark times out", test_timeout},
        .expectation = {},
    },
    {
        .input = {"a video frame's sample", test_sample_of_frame},
        .expectation = {},
    },
};

const CapturedCase CAPTURED_CASES[] = {
    {
        .input = {"PokemonBDSP", &BDSP_FADE_PROFILE},
        .expectation = {},
    },
    {
        .input = {"PokemonFRLG", &FRLG_FADE_PROFILE},
        .expectation = {},
    },
};

void add_tests_Transitions(UnitTestDatabase& database){
    for (const auto& item : TRACK_CASES){
        database.add<Test_TransitionTracker>(item);
    }
    for (const auto& item : TRANSITIONS_CASES){
        database.add<Test_Transitions>(item);
    }
    for (const auto& item : CAPTURED_CASES){
        database.add<Test_CapturedTransitions>(item);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
