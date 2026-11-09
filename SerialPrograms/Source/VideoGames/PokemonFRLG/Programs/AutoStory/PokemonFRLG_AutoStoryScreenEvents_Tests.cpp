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
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::ScreenEvent;
using Pokemon::ScreenEvents;

namespace{

struct ScreenEventsScenario{
    std::string name;
    //  The frame's colour; 0 for no frame.
    uint32_t color;
    ScreenEvents expected;
};

class Test_AutoStoryScreenEvents : public UnitTest{
public:
    Test_AutoStoryScreenEvents(ScreenEventsScenario scenario)
        : UnitTest("PokemonFRLG::AutoStoryScreenEvents - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        VideoSnapshot frame;
        if (m_scenario.color != 0){
            ImageRGB32 image(1920, 1080);
            image.fill(m_scenario.color);
            frame = VideoSnapshot(std::move(image), WallClock());
        }
        FrlgScreenObserver observer;
        const ScreenEvents got = observer.observe(frame, WallClock());
        if (got != m_scenario.expected){
            return std::format("expected {}, got {}", m_scenario.expected.describe(), got.describe());
        }
        return true;
    }

private:
    const ScreenEventsScenario m_scenario;
};

} // namespace


void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database){
    const std::vector<ScreenEventsScenario> scenarios = {
        {"no frame shows nothing", 0, {}},
        {"a black screen is a black fade only", 0xff000000, {ScreenEvent::FADE_BLACK}},
        {"a white screen is a white fade only", 0xffffffff, {ScreenEvent::FADE_WHITE}},
    };
    for (const ScreenEventsScenario& scenario : scenarios){
        database.add<Test_AutoStoryScreenEvents>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
