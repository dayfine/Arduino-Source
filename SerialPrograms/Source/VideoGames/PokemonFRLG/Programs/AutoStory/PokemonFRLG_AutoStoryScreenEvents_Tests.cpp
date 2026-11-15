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
#include <optional>
#include <string>
#include <utility>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonFRLG_AutoStoryFrameReplay_Tests.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"
#include "PokemonFRLG_AutoStoryScreenEvents_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct ScreenEventsCase{
    struct Input{
        //  The frame's colour; none for no frame.
        std::optional<uint32_t> color;
        //  The colour of a square in the middle of the frame, around the
        //  player; 0 for none.
        uint32_t middle = 0;
        //  The colour of the frame from the battle text box's top down;
        //  0 for none.
        uint32_t below = 0;
    };

    struct Expectation{
        ScreenEvents events;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

//  `color` over the pixels from (x0, y0) up to (x1, y1); none if it is 0.
void paint(ImageRGB32& image, uint32_t color, size_t x0, size_t y0, size_t x1, size_t y1){
    if (color == 0){
        return;
    }
    for (size_t y = y0; y < y1; y++){
        for (size_t x = x0; x < x1; x++){
            image.pixel(x, y) = color;
        }
    }
}

class Test_AutoStoryScreenEvents : public UnitTest{
public:
    explicit Test_AutoStoryScreenEvents(ScreenEventsCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryScreenEvents - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        VideoSnapshot frame;
        if (m_case.input.color.has_value()){
            ImageRGB32 image(1920, 1080);
            image.fill(*m_case.input.color);
            paint(image, m_case.input.below, 0, 740, 1920, 1080);
            paint(image, m_case.input.middle, 800, 380, 1120, 700);
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
        .name = "no frame shows nothing",
        .input = {.color = std::nullopt},
        .expectation = {{}},
    },
    {
        .name = "a black screen is a black fade only",
        .input = {.color = 0xff000000},
        .expectation = {{ScreenEvent::FADE_BLACK}},
    },
    {
        .name = "a white screen is a white fade only",
        .input = {.color = 0xffffffff},
        .expectation = {{ScreenEvent::FADE_WHITE}},
    },
    {
        //  Rock Tunnel without Flash (console B, 2026-11-09, 08.01): its
        //  black read as a battle's text box before each step.
        .name = "a dark cave, black but for a lit middle, shows nothing",
        .input = {.color = 0xff000000, .middle = 0xff8c7c64},
        .expectation = {{}},
    },
    {
        //  Silph Co. (console B, 2026-11-14 16.02, 2026-11-15 16.05): the
        //  black at a map's edge read as a battle's text box before each
        //  step, and the battle loop looked for a battle for 15 s.
        .name = "a lit map with black below, where the battle box would be, shows nothing",
        .input = {.color = 0xff88c8d8, .below = 0xff000000},
        .expectation = {{}},
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
