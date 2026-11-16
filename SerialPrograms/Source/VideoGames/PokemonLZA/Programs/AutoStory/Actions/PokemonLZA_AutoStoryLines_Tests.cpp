/*  PokemonLZA AutoStory - Lines Tests
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
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "PokemonLZA_AutoStoryLines.h"
#include "PokemonLZA_AutoStoryLines_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::Observation;
using Pokemon::ZonePose;


//  A look with `events` and the readings named in `readings`, `at` after
//  the script's start.
Observation<ZonePose> look(ScreenEvents events, std::vector<std::string_view> readings, Milliseconds at){
    Observation<ZonePose> observation;
    observation.when = WallClock::min() + at;
    observation.events = events;
    for (std::string_view reading : readings){
        observation.readings[std::string(reading)] = 1;
    }
    return observation;
}

Observation<ZonePose> dialog(Milliseconds at){
    return look({ScreenEvent::DIALOG_BOX}, {}, at);
}

Observation<ZonePose> card(Milliseconds at){
    return look({}, {TUTORIAL_CARD_READING}, at);
}

Observation<ZonePose> nothing(Milliseconds at){
    return look({}, {}, at);
}

Observation<ZonePose> party_bar(Milliseconds at){
    return look({ScreenEvent::PLAYER_CAN_MOVE}, {}, at);
}

std::string joined(const std::vector<std::string>& texts){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : ", ") + part;
    }
    return text;
}


struct LinesCase{
    std::string name;
    struct Input{
        std::vector<Observation<ZonePose>> looks;
        Milliseconds limit = Milliseconds(30000);
    };
    Input input;
    struct Expectation{
        bool text = false;
        bool card = false;
        bool timed_out = false;
        std::string inputs;
    };
    Expectation expectation;
};

//  All with 3 s of quiet.
const LinesCase LINES_CASES[] = {
    {
        .name = "a card after the text is seen, done 3 s after it",
        .input = {.looks = {dialog(Milliseconds(0)), card(Milliseconds(500)), nothing(Milliseconds(1000)), nothing(Milliseconds(4000))}},
        .expectation = {.text = true, .card = true, .timed_out = false, .inputs = "press A, wait 250ms, press A, wait 250ms, wait 250ms"},
    },
    {
        .name = "text with no card",
        .input = {.looks = {dialog(Milliseconds(0)), nothing(Milliseconds(3000))}},
        .expectation = {.text = true, .card = false, .timed_out = false, .inputs = "press A, wait 250ms"},
    },
    {
        .name = "nothing for 3 s presses nothing",
        .input = {.looks = {nothing(Milliseconds(0)), nothing(Milliseconds(3000))}},
        .expectation = {.text = false, .card = false, .timed_out = false, .inputs = "wait 250ms"},
    },
    {
        .name = "text past the limit times out",
        .input = {
            .looks = {dialog(Milliseconds(0)), dialog(Milliseconds(500)), dialog(Milliseconds(1000))},
            .limit = Milliseconds(1000),
        },
        .expectation = {.text = true, .card = false, .timed_out = true, .inputs = "press A, wait 250ms, press A, wait 250ms, press A"},
    },
};

UnitTestResult check_lines(const LinesCase& test, CancellableScope& scope){
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks}, scope);
    const LinesPassed passed = press_a_through_lines(backend, /*quiet=*/Milliseconds(3000), test.input.limit);
    const LinesCase::Expectation& expected = test.expectation;
    if (passed.text != expected.text || passed.card != expected.card || passed.timed_out != expected.timed_out){
        return std::format(
            "expected text {} card {} timed_out {}, got text {} card {} timed_out {}",
            expected.text, expected.card, expected.timed_out, passed.text, passed.card, passed.timed_out
        );
    }
    const std::string inputs = joined(backend.inputs());
    if (inputs != expected.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", expected.inputs, inputs);
    }
    return true;
}


struct MenuCase{
    std::string name;
    struct Input{
        std::vector<Observation<ZonePose>> looks;
        size_t presses = 0;
    };
    Input input;
    struct Expectation{
        bool out = false;
        std::string inputs;
    };
    Expectation expectation;
};

const MenuCase MENU_CASES[] = {
    {
        .name = "B until the party bar",
        .input = {.looks = {nothing(Milliseconds(0)), nothing(Milliseconds(0)), party_bar(Milliseconds(0))}, .presses = 5},
        .expectation = {.out = true, .inputs = "press B, press B"},
    },
    {
        .name = "B until the talk after the menu",
        .input = {.looks = {nothing(Milliseconds(0)), dialog(Milliseconds(0))}, .presses = 5},
        .expectation = {.out = true, .inputs = "press B"},
    },
    {
        .name = "still in the menu after the presses",
        .input = {.looks = {nothing(Milliseconds(0))}, .presses = 2},
        .expectation = {.out = false, .inputs = "press B, press B"},
    },
};

UnitTestResult check_menu(const MenuCase& test, CancellableScope& scope){
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks}, scope);
    const bool out = press_b_out_of_menu(backend, test.input.presses);
    if (out != test.expectation.out){
        return std::format("expected out of the menu: {}, got {}", test.expectation.out, out);
    }
    const std::string inputs = joined(backend.inputs());
    if (inputs != test.expectation.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", test.expectation.inputs, inputs);
    }
    return true;
}


class LinesTest : public UnitTest{
public:
    explicit LinesTest(LinesCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - press_a_through_lines: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_lines(m_case, scope);
    }
private:
    LinesCase m_case;
};

class MenuTest : public UnitTest{
public:
    explicit MenuTest(MenuCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - press_b_out_of_menu: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_menu(m_case, scope);
    }
private:
    MenuCase m_case;
};

} // namespace


void add_tests_AutoStoryLines(UnitTestDatabase& database){
    for (const LinesCase& test : LINES_CASES){
        database.add<LinesTest>(test);
    }
    for (const MenuCase& test : MENU_CASES){
        database.add<MenuTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
