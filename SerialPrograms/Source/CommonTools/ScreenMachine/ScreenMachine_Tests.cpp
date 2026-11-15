/*  Screen Machine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The screen machine on scripted observations: which state a look matches,
 *  a run that ends on its terminal state, the ways one fails (no state
 *  matches, a state comes up too often, the time or the looks run out, a
 *  waiting state stays still too long, steps that change nothing), and the
 *  trace's lines.
 *
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "ScreenMachine.h"
#include "ScreenMachine_Tests.h"

namespace PokemonAutomation{


namespace{

using namespace std::chrono_literals;


std::string joined(const std::vector<std::string>& items, std::string_view separator){
    std::string text;
    for (const std::string& item : items){
        text += text.empty() ? item : std::format("{}{}", separator, item);
    }
    return text;
}


//  One scripted look: its time from the first, what it shows, and its frame
//  as one uniform shade (none: no frame, as in a simulation).
struct Look{
    Milliseconds at = 0ms;
    ScreenEvents events = {};
    std::optional<uint32_t> shade = std::nullopt;
};

ScreenObservation observation_of(const Look& look, WallClock start){
    ScreenObservation observation;
    observation.when = start + look.at;
    observation.events = look.events;
    if (look.shade){
        ImageRGB32 image(16, 16);
        image.fill(*look.shade);
        observation.frame = VideoSnapshot(std::move(image), observation.when);
    }
    return observation;
}

//  `count` looks a second apart from `from` s, each showing `events` on
//  a frame of `shade`, if any.
std::vector<Look> looks_every_second(int from, int count, ScreenEvents events, std::optional<uint32_t> shade){
    std::vector<Look> looks;
    for (int second = from; second < from + count; second++){
        looks.push_back({.at = std::chrono::seconds(second), .events = events, .shade = shade});
    }
    return looks;
}

//  `first`, then `then`.
std::vector<Look> concat(std::vector<Look> first, const std::vector<Look>& then){
    first.insert(first.end(), then.begin(), then.end());
    return first;
}

constexpr uint32_t BLACK = 0xff000000;
constexpr uint32_t WHITE = 0xffffffff;


//  The states every case runs, in this order:
//  - "fade" on FADE_BLACK: waits, the screen may stay still up to 30 s;
//  - "choice" on CHOICE_BOX: terminal;
//  - "prompt" on INTERACT_PROMPT: a step, no visit limit;
//  - "text" on DIALOG_BOX: a step, at most 10 visits.
//  Each step's name goes to `acted`.
std::vector<ScreenState> test_states(std::vector<std::string>& acted){
    const auto state = [&acted](std::string name, ScreenEvent event){
        ScreenState result;
        result.name = name;
        result.matches = [event](const ScreenObservation& observation){
            return observation.events.has(event);
        };
        result.act = [&acted, name]{
            acted.push_back(name);
        };
        return result;
    };
    ScreenState fade = state("fade", ScreenEvent::FADE_BLACK);
    fade.act = nullptr;
    fade.max_still = 30s;
    ScreenState choice = state("choice", ScreenEvent::CHOICE_BOX);
    choice.terminal = true;
    ScreenState text = state("text", ScreenEvent::DIALOG_BOX);
    text.max_visits = 10;
    return {std::move(fade), std::move(choice), state("prompt", ScreenEvent::INTERACT_PROMPT), std::move(text)};
}


struct MatchCase{
    std::string name;
    struct Input{
        ScreenEvents events;
    };
    Input input;
    struct Expectation{
        //  The state matched; none when no state did.
        std::optional<std::string_view> state = std::nullopt;
    };
    Expectation expectation;
};

const MatchCase MATCH_CASES[] = {
    {
        .name = "the first state that matches wins",
        .input = {.events = {ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX}},
        .expectation = {.state = "choice"},
    },
    {
        .name = "one event",
        .input = {.events = {ScreenEvent::DIALOG_BOX}},
        .expectation = {.state = "text"},
    },
    {
        .name = "no state matches",
        .input = {.events = {ScreenEvent::BATTLE_STARTED}},
        .expectation = {},
    },
    {
        .name = "an empty look",
        .input = {.events = {}},
        .expectation = {},
    },
};

UnitTestResult check_match(const MatchCase& test){
    std::vector<std::string> acted;
    const std::vector<ScreenState> states = test_states(acted);
    ScreenObservation observation;
    observation.events = test.input.events;
    const ScreenState* state = match_screen_state(states, observation);
    const std::string got = state ? state->name : "none";
    if (got != test.expectation.state.value_or("none")){
        return std::format("matched {}", got);
    }
    return true;
}


struct RunCase{
    std::string name;
    struct Input{
        //  The looks, the last repeating.
        std::vector<Look> looks;
        Milliseconds timeout = 600s;
    };
    Input input;
    struct Expectation{
        //  The terminal state, or "failed: <failure>".
        std::string end;
        //  The steps taken, joined by ", "; none: not checked.
        std::optional<std::string> acted = std::nullopt;
        //  The trace's lines, joined by "; "; none: not checked.
        std::optional<std::string> trace = std::nullopt;
    };
    Expectation expectation;
};

const RunCase RUN_CASES[] = {
    {
        .name = "a run ends on its terminal state, after each step",
        .input = {.looks = {
            {.at = 0s, .events = {ScreenEvent::INTERACT_PROMPT}},
            {.at = 1s, .events = {ScreenEvent::DIALOG_BOX}},
            {.at = 2s, .events = {ScreenEvent::CHOICE_BOX}},
        }},
        .expectation = {.end = "choice", .acted = "prompt, text, choice"},
    },
    {
        .name = "the trace logs each change of screen once, timed from the first look",
        .input = {.looks = {
            {.at = 0s, .events = {ScreenEvent::INTERACT_PROMPT}},
            {.at = 1s, .events = {ScreenEvent::INTERACT_PROMPT}},
            {.at = 2500ms, .events = {ScreenEvent::DIALOG_BOX}},
            {.at = 3s, .events = {ScreenEvent::CHOICE_BOX}},
        }},
        .expectation = {
            .end = "choice",
            .trace = "[test] screen 1: prompt at 0.0s; [test] screen 2: text at 2.5s; [test] screen 3: choice at 3.0s",
        },
    },
    {
        .name = "no state matches",
        .input = {.looks = {{.at = 0s, .events = {ScreenEvent::BATTLE_STARTED}}}},
        .expectation = {.end = "failed: The screen matches none of the known states.", .acted = ""},
    },
    {
        .name = "a state comes up more often than its limit",
        .input = {.looks = looks_every_second(0, 1, {ScreenEvent::DIALOG_BOX}, std::nullopt)},
        .expectation = {.end = "failed: Screen \"text\" came up more than 10 times: the step for it is not working."},
    },
    {
        .name = "the looks' time runs out",
        .input = {
            .looks = {
                {.at = 0s, .events = {ScreenEvent::DIALOG_BOX}},
                {.at = 1s, .events = {ScreenEvent::INTERACT_PROMPT}},
                {.at = 2s, .events = {ScreenEvent::DIALOG_BOX}},
            },
            .timeout = 1s,
        },
        .expectation = {.end = "failed: Did not finish within 1s.", .acted = "text, prompt"},
    },
    {
        .name = "looks with no time and a state with no limit run out of looks",
        .input = {.looks = {{.at = 0s, .events = {ScreenEvent::INTERACT_PROMPT}}}},
        .expectation = {.end = "failed: Did not finish within 10000 looks."},
    },
    {
        .name = "a waiting state may stay still up to its limit",
        .input = {.looks = concat(
            looks_every_second(0, 12, {ScreenEvent::FADE_BLACK}, BLACK),
            {{.at = 12s, .events = {ScreenEvent::CHOICE_BOX}, .shade = WHITE}}
        )},
        .expectation = {.end = "choice", .acted = "choice"},
    },
    {
        .name = "a waiting state still past its limit fails",
        .input = {.looks = looks_every_second(0, 40, {ScreenEvent::FADE_BLACK}, BLACK)},
        .expectation = {.end = "failed: The screen stayed still on \"fade\" for more than 30s."},
    },
    {
        .name = "steps that change nothing on screen stall",
        .input = {.looks = looks_every_second(0, 12, {ScreenEvent::DIALOG_BOX}, WHITE)},
        .expectation = {
            .end = "failed: The screen stopped changing: 8 steps on \"text\" did nothing.",
            .acted = "text, text, text, text, text, text, text, text",
        },
    },
    {
        .name = "a changed frame starts the stall count again",
        .input = {.looks = concat(
            concat(
                looks_every_second(0, 5, {ScreenEvent::DIALOG_BOX}, WHITE),
                looks_every_second(5, 5, {ScreenEvent::DIALOG_BOX}, BLACK)
            ),
            {{.at = 10s, .events = {ScreenEvent::CHOICE_BOX}, .shade = WHITE}}
        )},
        .expectation = {.end = "choice"},
    },
    {
        .name = "looks with no frame never stall",
        .input = {.looks = concat(
            looks_every_second(0, 9, {ScreenEvent::DIALOG_BOX}, std::nullopt),
            {{.at = 9s, .events = {ScreenEvent::CHOICE_BOX}}}
        )},
        .expectation = {.end = "choice"},
    },
};

UnitTestResult check_run(const RunCase& test, CancellableScope& scope){
    std::vector<std::string> acted;
    const std::vector<ScreenState> states = test_states(acted);
    std::vector<std::string> lines;
    StepTrace trace("test", [&lines](const std::string& line){
        lines.push_back(line);
    });
    const WallClock start = current_time();
    size_t next = 0;
    const std::function<ScreenObservation()> observe = [&]{
        const Look& look = test.input.looks[std::min(next, test.input.looks.size() - 1)];
        next++;
        return observation_of(look, start);
    };
    const ScreenMachineResult result = run_screen_machine(observe, scope, states, test.input.timeout, trace);
    const std::string end = result.succeeded() ? result.terminal : std::format("failed: {}", result.failure);
    if (end != test.expectation.end){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.end, end);
    }
    if (test.expectation.acted && joined(acted, ", ") != *test.expectation.acted){
        return std::format("expected the steps \"{}\", got \"{}\"", *test.expectation.acted, joined(acted, ", "));
    }
    if (test.expectation.trace && joined(lines, "; ") != *test.expectation.trace){
        return std::format("expected the trace \"{}\", got \"{}\"", *test.expectation.trace, joined(lines, "; "));
    }
    return true;
}


class Test_MatchScreenState : public UnitTest{
public:
    explicit Test_MatchScreenState(const MatchCase& test)
        : UnitTest(std::format("CommonTools::ScreenMachine - match: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_match(m_test);
    }

private:
    const MatchCase& m_test;
};

class Test_RunScreenMachine : public UnitTest{
public:
    explicit Test_RunScreenMachine(const RunCase& test)
        : UnitTest(std::format("CommonTools::ScreenMachine - run: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_run(m_test, scope);
    }

private:
    const RunCase& m_test;
};


} // namespace


void add_tests_ScreenMachine(UnitTestDatabase& database){
    for (const MatchCase& test : MATCH_CASES){
        database.add<Test_MatchScreenState>(test);
    }
    for (const RunCase& test : RUN_CASES){
        database.add<Test_RunScreenMachine>(test);
    }
}



}  // namespace PokemonAutomation
