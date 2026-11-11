/*  Screen Events Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The shared event vocabulary and the watcher form of an observer.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Pokemon_ScreenEvents.h"
#include "Pokemon_ScreenEvents_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

using namespace std::chrono_literals;

UnitTestResult expect_equal(const std::string& what, const std::string& got, const std::string& expected){
    if (got == expected){
        return true;
    }
    return std::format("{}: expected \"{}\", got \"{}\"", what, expected, got);
}


UnitTestResult test_set_and_describe(){
    ScreenEvents events{ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE};
    if (!events.has(ScreenEvent::DIALOG_BOX) || events.has(ScreenEvent::CHOICE_BOX)){
        return "has() disagrees with the constructor's events";
    }
    events.set(ScreenEvent::DIALOG_BOX, false);
    events.merge(fade_events(WarpFade::WHITE));
    events.merge(fade_events(WarpFade::NONE));
    if (fade_in(events) != WarpFade::WHITE){
        return "fade_events(WHITE) did not give a white fade";
    }
    if (ScreenEvents::from_bits(events.bits()) != events){
        return "from_bits(bits()) is not the same set";
    }
    if (describe(ScreenEvents()) != "nothing"){
        return "an empty set does not describe as nothing";
    }
    return expect_equal("describe()", describe(events), "white fade, player can move");
}

//  A screen filled with one colour, or a checkerboard of two.
ImageRGB32 screen_of(uint32_t a, uint32_t b){
    ImageRGB32 out(64, 36);
    for (size_t y = 0; y < out.height(); y++){
        for (size_t x = 0; x < out.width(); x++){
            out.pixel(x, y) = (x + y) % 2 == 0 ? a : b;
        }
    }
    return out;
}

UnitTestResult test_fade_of_screen(){
    struct Row{
        std::string name;
        uint32_t a;
        uint32_t b;
        WarpFade fade;
    };
    const std::vector<Row> rows = {
        {"black", 0xff000000, 0xff000000, WarpFade::BLACK},
        {"a cave mouth's dim fade (60 a channel)", 0xff3c3c3c, 0xff3c3c3c, WarpFade::BLACK},
        {"white", 0xffffffff, 0xffffffff, WarpFade::WHITE},
        {"grey, neither", 0xff787878, 0xff787878, WarpFade::NONE},
        {"a dark textured floor", 0xff101010, 0xff505050, WarpFade::NONE},
    };
    for (const Row& row : rows){
        const WarpFade got = fade_of_screen(screen_of(row.a, row.b));
        if (got != row.fade){
            return std::format("{}: expected {}, got {}", row.name, warp_fade_name(row.fade), warp_fade_name(got));
        }
    }
    return true;
}

//  Replays a fixed list of event sets, one per frame.
class ScriptedObserver : public ScreenEventObserver{
public:
    explicit ScriptedObserver(std::vector<ScreenEvents> frames)
        : m_frames(std::move(frames))
    {}
    virtual ScreenEvents observe(const VideoSnapshot&, WallClock) override{
        return m_next < m_frames.size() ? m_frames[m_next++] : ScreenEvents();
    }
    virtual void make_overlays(VideoOverlaySet&) const override{}

private:
    std::vector<ScreenEvents> m_frames;
    size_t m_next = 0;
};

UnitTestResult test_watcher(){
    ScriptedObserver observer({
        {ScreenEvent::PLAYER_CAN_MOVE},
        {ScreenEvent::FADE_BLACK},
        {ScreenEvent::LOCATION_BANNER, ScreenEvent::PLAYER_CAN_MOVE},
        {ScreenEvent::DIALOG_BOX},
    });
    ScreenEventWatcher watcher(observer, {ScreenEvent::LOCATION_BANNER});
    const VideoSnapshot frame;
    std::vector<bool> stops;
    for (size_t c = 0; c < 4; c++){
        stops.push_back(watcher.process_frame(frame));
    }
    if (stops != std::vector<bool>{false, false, true, false}){
        return "the watcher did not stop exactly on the banner frame";
    }
    UnitTestResult result = expect_equal("seen()", describe(watcher.seen()),
        "black fade, location banner, dialog box, player can move");
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    result = expect_equal("stopped_on()", describe(watcher.stopped_on()), "location banner, player can move");
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    watcher.reset();
    return expect_equal("seen() after reset()", describe(watcher.seen()), "nothing");
}


struct ScreenEventsInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct ScreenEventsExpectation{
};

struct ScreenEventsCase{
    ScreenEventsInput input;
    ScreenEventsExpectation expectation;
};

class Test_ScreenEvents : public UnitTest{
public:
    explicit Test_ScreenEvents(ScreenEventsCase test_case)
        : UnitTest(std::format("Pokemon::ScreenEvents - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.body();
    }

private:
    ScreenEventsCase m_case;
};

} // namespace


const ScreenEventsCase SCREEN_EVENTS_CASES[] = {
    {
        .input = {"a set of events, its fade and its log text", test_set_and_describe},
        .expectation = {},
    },
    {
        .input = {"a fade is uniform, dark or white; a textured floor is none", test_fade_of_screen},
        .expectation = {},
    },
    {
        .input = {"an observer as a watcher records and stops", test_watcher},
        .expectation = {},
    },
};

void add_tests_ScreenEvents(UnitTestDatabase& database){
    for (const auto& item : SCREEN_EVENTS_CASES){
        database.add<Test_ScreenEvents>(item);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
