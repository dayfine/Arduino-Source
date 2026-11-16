/*  Ball Throw
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "PokemonLZA_BallThrow.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;



BallScrolls ball_scrolls(int8_t right_scrolls){
    if (right_scrolls >= 0){
        return {DPAD_RIGHT, (size_t)right_scrolls};
    }
    return {DPAD_LEFT, (size_t)(-(int)right_scrolls)};
}

Milliseconds throw_buttons_hold(const BallThrowOptions& options){
    const BallScrolls scrolls = ball_scrolls(options.right_scrolls);
    return options.aim + (options.scroll_hold + options.scroll_release) * (Milliseconds::rep)scrolls.count;
}

void throw_ball(ProControllerContext& context, const BallThrowOptions& options){
    if (options.lock_on > 0ms){
        pbf_controller_state(context, BUTTON_ZL, DPAD_NONE, {0, 0}, {0, 0}, options.lock_on);
    }

    //  The D-pad presses start once the aim is up, while ZL + ZR stay held.
    ssf_press_button(
        context,
        BUTTON_ZL | BUTTON_ZR,
        options.aim, throw_buttons_hold(options),
        0ms
    );
    const BallScrolls scrolls = ball_scrolls(options.right_scrolls);
    for (size_t c = 0; c < scrolls.count; c++){
        pbf_press_dpad(context, scrolls.direction, options.scroll_hold, options.scroll_release);
    }

    if (options.after_throw > 0ms){
        pbf_wait(context, options.after_throw);
    }
}



namespace{

struct BallScrollsCase{
    std::string name;
    struct Input{
        int8_t right_scrolls = 0;
    } input = {};
    struct Expectation{
        DpadPosition direction = DPAD_NONE;
        size_t count = 0;
    } expected = {};
};

class Test_BallScrolls : public UnitTest{
public:
    Test_BallScrolls(BallScrollsCase test_case)
        : UnitTest(std::format("PokemonLZA::BallThrow::ball_scrolls - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const BallScrolls actual = ball_scrolls(m_case.input.right_scrolls);
        if (actual.direction != m_case.expected.direction){
            return UnitTestResult("Wrong scroll direction.");
        }
        if (actual.count != m_case.expected.count){
            return UnitTestResult(std::format(
                "Expected {} scrolls, got {}.",
                m_case.expected.count, actual.count
            ));
        }
        return UnitTestResult(true);
    }

private:
    BallScrollsCase m_case;
};


struct ThrowButtonsHoldCase{
    std::string name;
    struct Input{
        BallThrowOptions options = {};
    } input = {};
    struct Expectation{
        Milliseconds hold = 0ms;
    } expected = {};
};

class Test_ThrowButtonsHold : public UnitTest{
public:
    Test_ThrowButtonsHold(ThrowButtonsHoldCase test_case)
        : UnitTest(std::format("PokemonLZA::BallThrow::throw_buttons_hold - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const Milliseconds actual = throw_buttons_hold(m_case.input.options);
        if (actual != m_case.expected.hold){
            return UnitTestResult(std::format(
                "Expected ZL + ZR held {} ms, got {} ms.",
                m_case.expected.hold.count(), actual.count()
            ));
        }
        return UnitTestResult(true);
    }

private:
    ThrowButtonsHoldCase m_case;
};


BallThrowOptions scrolling(int8_t right_scrolls){
    BallThrowOptions options;
    options.right_scrolls = right_scrolls;
    return options;
}

}  // namespace



void add_tests_BallThrow(UnitTestDatabase& database){
    const std::vector<BallScrollsCase> scroll_cases{
        {"no scroll", {0}, {DPAD_RIGHT, 0}},
        {"right", {3}, {DPAD_RIGHT, 3}},
        {"left", {-2}, {DPAD_LEFT, 2}},
        {"far left", {-128}, {DPAD_LEFT, 128}},
    };
    for (const BallScrollsCase& test_case : scroll_cases){
        database.add<Test_BallScrolls>(test_case);
    }

    const std::vector<ThrowButtonsHoldCase> hold_cases{
        {"no scroll is the aim alone", {scrolling(0)}, {500ms}},
        {"each right scroll adds hold + release", {scrolling(2)}, {500ms + 2 * 200ms}},
        {"each left scroll adds hold + release", {scrolling(-3)}, {500ms + 3 * 200ms}},
    };
    for (const ThrowButtonsHoldCase& test_case : hold_cases){
        database.add<Test_ThrowButtonsHold>(test_case);
    }
}



}  // namespace PokemonLZA
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
