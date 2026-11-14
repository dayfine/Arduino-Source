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
    ScreenEventsInput input;
    ScreenEventsExpectation expectation;
};

class Test_AutoStoryScreenEvents : public ImageUnitTest{
public:
    explicit Test_AutoStoryScreenEvents(ScreenEventsCase test_case)
        : ImageUnitTest(
            std::format("PokemonLZA::AutoStoryScreenEvents - {}", test_case.input.frame),
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

} // namespace


const ScreenEventsCase SCREEN_EVENTS_CASES[] = {
    {
        .input = {"za2_m3o_s1.png"},
        .expectation = {{ScreenEvent::INTERACT_PROMPT, ScreenEvent::PLAYER_CAN_MOVE}},
    },
    {
        .input = {"za2_m3p_s1.png"},
        .expectation = {{ScreenEvent::INTERACT_PROMPT, ScreenEvent::PLAYER_CAN_MOVE}},
    },
    {
        .input = {"za2_m3v_s1.png"},
        .expectation = {{ScreenEvent::INTERACT_PROMPT}},
    },
    {
        .input = {"za2_m2q_s1.png"},
        .expectation = {{ScreenEvent::PLAYER_CAN_MOVE}},
    },
    {
        .input = {"za2_hud_s3.png"},
        .expectation = {{ScreenEvent::BATTLE_STARTED}},
    },
    {
        .input = {"za2_m2n_s1.png"},
        .expectation = {{ScreenEvent::BATTLE_STARTED}},
    },
    {
        //  The battle tutorial's card, before the HUD.
        .input = {"za2_hud_s1.png"},
        .expectation = {{}},
    },
    {
        //  A white box while its text still types: no advance arrow yet.
        .input = {"za2_m3s_s1.png"},
        .expectation = {{ScreenEvent::DIALOG_BOX}},
    },
    {
        .input = {"za2_fight2_s1.png"},
        .expectation = {{ScreenEvent::DIALOG_BOX}},
    },
    {
        .input = {"za2_m3w_s2.png"},
        .expectation = {{ScreenEvent::CHOICE_BOX}},
    },
    {
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
