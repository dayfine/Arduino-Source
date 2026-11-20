/*  PokemonLZA AutoStory - Menu save Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryMenuSave_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{



ScreenObservation look(ScreenEvents events, std::vector<std::string_view> readings, Milliseconds at){
    ScreenObservation observation;
    observation.when = WallClock::min() + at;
    observation.events = events;
    for (std::string_view reading : readings){
        observation.readings[std::string(reading)] = 1;
    }
    return observation;
}

ScreenObservation overworld(Milliseconds at){
    return look({ScreenEvent::PLAYER_CAN_MOVE}, /*readings=*/{}, at);
}

ScreenObservation card(Milliseconds at){
    return look(/*events=*/{}, {TUTORIAL_CARD_READING}, at);
}

//  01.01's platform: the controls hint, no party bar.
ScreenObservation platform(Milliseconds at){
    return look(/*events=*/{}, {CONTROLS_HINT_READING}, at);
}

//  No detector reads the menu or options screens.
ScreenObservation options(Milliseconds at){
    return look(/*events=*/{}, /*readings=*/{}, at);
}

std::string non_poll_inputs(const std::vector<std::string>& texts){
    std::string text;
    for (const std::string& part : texts){
        if (part == "wait 250ms"){
            continue;
        }
        if (!text.empty()){
            text += ", ";
        }
        text += part;
    }
    return text;
}


struct TextSpeedCase{
    std::string name;
    struct Input{
        std::vector<ScreenObservation> looks;
    };
    Input input;
    struct Expectation{
        //  Empty: ok.
        std::string failure;
        std::string inputs;
    };
    Expectation expectation;
};

//  The blind presses from the main menu to the text speed, set to Fast.
constexpr std::string_view TEXT_SPEED_ROWS = "press L, press Up, press A, press Down, press Right";

//  X, then the menu's 2 s.
constexpr std::string_view TEXT_SPEED_OPEN = "press X, wait 2000ms";

const TextSpeedCase TEXT_SPEED_CASES[] = {
    {
        //  console B, 2026-11-19, 01.01 3/3: the card read on two looks
        //  while it faded out, got a second A on the main menu, and the bag
        //  opened.
        .name = "a tutorial card fading out after its A gets no second A",
        .input = {.looks = {
            card(Milliseconds(0)), options(Milliseconds(1500)),
            options(Milliseconds(2500)), platform(Milliseconds(3500)), platform(Milliseconds(3750)),
        }},
        .expectation = {
            .failure = "",
            .inputs = std::format("{}, press A, wait 1500ms, {}, press B", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
    {
        .name = "a menu with no tutorial card gets the rows at once",
        .input = {.looks = {options(Milliseconds(0)), overworld(Milliseconds(1000)), overworld(Milliseconds(1250))}},
        .expectation = {
            .failure = "",
            .inputs = std::format("{}, {}", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
    {
        .name = "a screen no detector reads after the rows, like a stray bag, gets B until the overworld",
        .input = {.looks = {
            options(Milliseconds(0)), options(Milliseconds(1000)), options(Milliseconds(2000)),
            options(Milliseconds(3000)), overworld(Milliseconds(4000)), overworld(Milliseconds(4250)),
        }},
        .expectation = {
            .failure = "",
            .inputs = std::format("{}, {}, press B, press B, press B", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
    {
        .name = "never back on the overworld: the step times out",
        .input = {.looks = {options(Milliseconds(0)), options(Milliseconds(1000)), options(Milliseconds(31500))}},
        .expectation = {
            .failure = "text_speed_timeout",
            .inputs = std::format("{}, {}, press B", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
};

UnitTestResult check_text_speed(const TextSpeedCase& test, CancellableScope& scope){
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    const std::optional<Pokemon::StoryFailure> outcome = set_text_speed_fast(backend);
    if ((outcome ? outcome->code : std::string()) != test.expectation.failure){
        return std::format("expected failure \"{}\", got \"{}\"", test.expectation.failure, (outcome ? std::format("{}", *outcome) : std::string("ok")));
    }
    const std::string inputs = non_poll_inputs(backend.inputs());
    if (inputs != test.expectation.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", test.expectation.inputs, inputs);
    }
    return true;
}

class TextSpeedTest : public UnitTest{
public:
    explicit TextSpeedTest(TextSpeedCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - set_text_speed_fast: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_text_speed(m_case, scope);
    }
private:
    TextSpeedCase m_case;
};

} // namespace


void add_tests_AutoStoryMenuSave(UnitTestDatabase& database){
    for (const TextSpeedCase& test : TEXT_SPEED_CASES){
        database.add<TextSpeedTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
