/*  FRLG AutoStory - Screen Events Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The adapter on synthetic frames: a uniform screen is a fade and nothing
 *  else, and no frame shows nothing. Frames of real boxes and battles are
 *  the detectors' own image tests (CommandLineTests); a flat grey frame is
 *  not one of those (the battle text box reads it as a box).
 *
 */

#include <format>
#include <string>
#include <utility>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonFRLG_AutoStoryFrameReplay_Tests.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::ScreenEvent;
using Pokemon::ScreenEvents;

namespace{

struct ScreenEventsInput{
    std::string name;
    //  The frame's colour; 0 for no frame.
    uint32_t color;
};

struct ScreenEventsExpectation{
    ScreenEvents events;
};

struct ScreenEventsCase{
    ScreenEventsInput input;
    ScreenEventsExpectation expectation;
};

class Test_AutoStoryScreenEvents : public UnitTest{
public:
    explicit Test_AutoStoryScreenEvents(ScreenEventsCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryScreenEvents - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        VideoSnapshot frame;
        if (m_case.input.color != 0){
            ImageRGB32 image(1920, 1080);
            image.fill(m_case.input.color);
            frame = VideoSnapshot(std::move(image), WallClock());
        }
        FrlgScreenObserver observer;
        const ScreenEvents got = observer.observe(frame, WallClock());
        if (got != m_case.expectation.events){
            return std::format("expected {}, got {}", describe(m_case.expectation.events), describe(got));
        }
        return true;
    }

private:
    ScreenEventsCase m_case;
};

} // namespace


const ScreenEventsCase SCREEN_EVENTS_CASES[] = {
    {
        .input = {"no frame shows nothing", 0},
        .expectation = {{}},
    },
    {
        .input = {"a black screen is a black fade only", 0xff000000},
        .expectation = {{ScreenEvent::FADE_BLACK}},
    },
    {
        .input = {"a white screen is a white fade only", 0xffffffff},
        .expectation = {{ScreenEvent::FADE_WHITE}},
    },
};

void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database){
    for (const auto& item : SCREEN_EVENTS_CASES){
        database.add<Test_AutoStoryScreenEvents>(item);
    }
    //  The same events on recorded console frames.
    add_tests_AutoStoryFrameReplay(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
