/*  PLZA AutoStory - Screen Events Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The adapter on lab-B frames from 2026-11-08 (1920x1080), kept in
 *  CommandLineTests under PokemonLZA/AutoStoryScreenEvents/.
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "Tests/ImageUnitTest.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

struct ScreenEventsInput{
    //  The frame, under PokemonLZA/AutoStoryScreenEvents/.
    std::string frame;
};

struct ScreenEventsExpectation{
    ScreenEvents events;
};

struct ScreenEventsCase{
    std::string name;
    ScreenEventsInput input;
    ScreenEventsExpectation expectation;
};

//  The observer on one frame.
class FrameEventsTest : public ImageUnitTest{
public:
    explicit FrameEventsTest(ScreenEventsCase test_case)
        : ImageUnitTest(
            std::format("PokemonLZA::AutoStoryScreenEvents - {}: {}", test_case.input.frame, test_case.name),
            {std::format("PokemonLZA/AutoStoryScreenEvents/{}", test_case.input.frame)}
        )
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run_on_images(
        Logger&, CancellableScope&,
        const std::vector<ImageRGB32>& images
    ) const override{
        PlzaScreenObserver observer;
        const ScreenEvents got = observer.observe(VideoSnapshot(images[0].copy(), WallClock()), WallClock());
        if (got != m_case.expectation.events){
            return std::format("expected {}, got {}", describe(m_case.expectation.events), describe(got));
        }
        return true;
    }

private:
    ScreenEventsCase m_case;
};

//  FrameEventsTest, failing with the frame's path when the frame is not in
//  CommandLineTests (a frame lost before its upload needs a console
//  recapture), instead of the image loader's bare FileException.
class Test_AutoStoryScreenEvents : public UnitTest{
public:
    explicit Test_AutoStoryScreenEvents(ScreenEventsCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryScreenEvents - {}: {}", test_case.input.frame, test_case.name))
        , m_frame(std::move(test_case))
    {
        m_memory = m_frame.memory();
    }

    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        const std::string& frame = m_frame.image_paths()[0];
        if (!Filesystem::exists(UNIT_TEST_RESOURCE_PATH() + frame)){
            return std::format("Missing frame {} in CommandLineTests: it needs a recapture.", frame);
        }
        return m_frame.run(logger, scope);
    }

private:
    FrameEventsTest m_frame;
};

} // namespace


const ScreenEventsCase SCREEN_EVENTS_CASES[] = {
    {
        .name = "an A prompt on free control",
        .input = {"za2_m3o_s1.png"},
        .expectation = {{ScreenEvent::INTERACT_PROMPT, ScreenEvent::PLAYER_CAN_MOVE}},
    },
    {
        .name = "an A prompt on free control, another spot",
        .input = {"za2_m3p_s1.png"},
        .expectation = {{ScreenEvent::INTERACT_PROMPT, ScreenEvent::PLAYER_CAN_MOVE}},
    },
    {
        .name = "an A prompt, control not free",
        .input = {"za2_m3v_s1.png"},
        .expectation = {{ScreenEvent::INTERACT_PROMPT}},
    },
    {
        .name = "free control",
        .input = {"za2_m2q_s1.png"},
        .expectation = {{ScreenEvent::PLAYER_CAN_MOVE}},
    },
    {
        .name = "the battle HUD",
        .input = {"za2_hud_s3.png"},
        .expectation = {{ScreenEvent::BATTLE_STARTED}},
    },
    {
        .name = "a battle",
        .input = {"za2_m2n_s1.png"},
        .expectation = {{ScreenEvent::BATTLE_STARTED}},
    },
    {
        //  The battle tutorial's card, before the HUD.
        .name = "the battle tutorial card, no event",
        .input = {"za2_hud_s1.png"},
        .expectation = {{}},
    },
    {
        //  A white box while its text still types: no advance arrow yet.
        .name = "a box still typing",
        .input = {"za2_m3s_s1.png"},
        .expectation = {{ScreenEvent::DIALOG_BOX}},
    },
    {
        .name = "a box in a fight",
        .input = {"za2_fight2_s1.png"},
        .expectation = {{ScreenEvent::DIALOG_BOX}},
    },
    {
        .name = "a choice",
        .input = {"za2_m3w_s2.png"},
        .expectation = {{ScreenEvent::CHOICE_BOX}},
    },
    {
        .name = "a black fade",
        .input = {"za2_m2m_s1.png"},
        .expectation = {{ScreenEvent::FADE_BLACK}},
    },
};

void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database){
    for (const auto& item : SCREEN_EVENTS_CASES){
        database.add<Test_AutoStoryScreenEvents>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
