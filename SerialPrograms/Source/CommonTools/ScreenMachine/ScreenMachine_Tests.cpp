/*  Screen Machine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The screen machine run on scripted observations: which state a look
 *  matches, a run that ends on its terminal state, one that fails (no state
 *  matches, a state comes up too often, the time runs out), and the still
 *  checks (a waiting state's limit, a step that changes nothing).
 *
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
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


std::string joined(const std::vector<std::string>& items){
    std::string text;
    for (const std::string& item : items){
        text += text.empty() ? item : ", " + item;
    }
    return text;
}


//  What a look showed, matched against states that match on observations.
struct MatchCase{
    std::string_view name;
    struct Input{
        std::vector<ScreenEvent> events;
    };

    //  The state matched; none when no state did.
    struct Expectation{
        std::optional<std::string_view> state = std::nullopt;
    };

    Input input;
    Expectation expectation;
};

const MatchCase MATCH_CASES[] = {
    {.name = "the first state that matches", .input = {.events = {ScreenEvent::BATTLE_STARTED, ScreenEvent::DIALOG_BOX}}, .expectation = {.state = "battle"}},
    {.name = "text", .input = {.events = {ScreenEvent::DIALOG_BOX}}, .expectation = {.state = "text"}},
    {.name = "overworld", .input = {.events = {ScreenEvent::PLAYER_CAN_MOVE}}, .expectation = {.state = "overworld"}},
    {.name = "an image-only state is skipped", .input = {.events = {ScreenEvent::FADE_WHITE}}, .expectation = {}},
};

ScreenState observation_state(std::string name, ScreenEvent event){
    ScreenState state;
    state.name = std::move(name);
    state.matches_observation = [event](const ScreenObservation& observation){
        return observation.events.has(event);
    };
    state.max_visits = 10;
    return state;
}

class Test_MatchScreenState : public UnitTest{
public:
    explicit Test_MatchScreenState(MatchCase test)
        : UnitTest(std::format("CommonTools::ScreenMachine - match: {}", test.name))
        , m_case(std::move(test))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<ScreenState> states{
            observation_state("battle", ScreenEvent::BATTLE_STARTED),
            observation_state("text", ScreenEvent::DIALOG_BOX),
            observation_state("overworld", ScreenEvent::PLAYER_CAN_MOVE),
        };
        ScreenState image_only;
        image_only.name = "image_only";
        image_only.matches = [](const ImageViewRGB32&){
            return true;
        };
        states.push_back(std::move(image_only));
        ScreenObservation observation;
        for (ScreenEvent event : m_case.input.events){
            observation.events.set(event, true);
        }
        const ScreenState* state = match_screen_state(states, observation);
        const std::string got = state ? state->name : "none";
        if (got != m_case.expectation.state.value_or("none")){
            return std::format("matched {}", got);
        }
        return true;
    }

private:
    MatchCase m_case;
};


//  States for the choice, prompt and text events; the choice ends the run.
std::vector<ScreenState> choice_states(std::vector<std::string>& acted){
    const auto state = [&acted](std::string name, ScreenEvent event){
        ScreenState result;
        result.name = name;
        result.matches_observation = [event](const ScreenObservation& observation){
            return observation.events.has(event);
        };
        result.act = [&acted, name]{
            acted.push_back(name);
        };
        result.max_visits = 3;
        return result;
    };
    std::vector<ScreenState> states{
        state("choice", ScreenEvent::CHOICE_BOX),
        state("prompt", ScreenEvent::INTERACT_PROMPT),
        state("text", ScreenEvent::DIALOG_BOX),
    };
    states[0].terminal = true;
    return states;
}

//  The machine over `events`, one look each, the last repeating, a second
//  apart.
ScreenMachineResult run_on_events(
    const std::vector<ScreenEvent>& events, CancellableScope& scope, Milliseconds timeout,
    std::vector<std::string>& acted, std::vector<std::string>& lines
){
    const std::vector<ScreenState> states = choice_states(acted);
    StepTrace trace("test", [&lines](const std::string& line){
        lines.push_back(line);
    });
    size_t next = 0;
    const WallClock start = current_time();
    const std::function<ScreenObservation()> observe = [&]{
        ScreenObservation observation;
        observation.when = start + std::chrono::seconds(next);
        observation.events.set(events[std::min(next, events.size() - 1)], true);
        next++;
        return observation;
    };
    return run_screen_machine_on_observations(observe, scope, states, timeout, trace);
}

UnitTestResult observation_machine_ends(CancellableScope& scope){
    std::vector<std::string> acted;
    std::vector<std::string> lines;
    const ScreenMachineResult result = run_on_events(
        {ScreenEvent::INTERACT_PROMPT, ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX}, scope, 60s, acted, lines
    );
    if (!result.succeeded() || result.terminal != "choice"){
        return std::format("expected the run to end on choice, got \"{}\"", result.failure);
    }
    if (joined(acted) != "prompt, text, choice"){
        return std::format("expected the states to act in order, got {}", joined(acted));
    }
    if (lines.size() != 3 || !lines[0].starts_with("[test] screen 1: prompt at ")){
        return std::format("expected a trace line per screen, got {}", joined(lines));
    }
    return true;
}

UnitTestResult observation_machine_fails(CancellableScope& scope){
    std::vector<std::string> acted;
    std::vector<std::string> lines;
    ScreenMachineResult result = run_on_events({ScreenEvent::BATTLE_STARTED}, scope, 60s, acted, lines);
    if (result.failure != "The screen matches none of the known states."){
        return std::format("expected no state to match, got \"{}\"", result.failure);
    }
    result = run_on_events({ScreenEvent::DIALOG_BOX}, scope, 60s, acted, lines);
    if (!result.failure.starts_with("Screen \"text\" came up more than 3 times")){
        return std::format("expected text to come up too often, got \"{}\"", result.failure);
    }
    result = run_on_events({ScreenEvent::DIALOG_BOX, ScreenEvent::INTERACT_PROMPT, ScreenEvent::DIALOG_BOX}, scope, 1s, acted, lines);
    if (result.failure != "Did not finish within 1s."){
        return std::format("expected the looks' time to run out, got \"{}\"", result.failure);
    }
    return true;
}

//  A look `seconds` after `start` on a uniform frame of `shade`, showing
//  `event`.
ScreenObservation framed_look(WallClock start, int seconds, uint32_t shade, ScreenEvent event){
    ScreenObservation observation;
    observation.when = start + std::chrono::seconds(seconds);
    observation.events.set(event, true);
    ImageRGB32 image(16, 16);
    image.fill(shade);
    observation.frame = VideoSnapshot(std::move(image), observation.when);
    return observation;
}

//  A fade that waits (still up to 30 s), a box whose step is a press, and
//  the overworld that ends the run, over `looks`, the last repeating.
ScreenMachineResult run_still_states(const std::vector<ScreenObservation>& looks, CancellableScope& scope){
    ScreenState fade;
    fade.name = "fade";
    fade.matches_observation = [](const ScreenObservation& observation){
        return observation.events.has(ScreenEvent::FADE_BLACK);
    };
    fade.max_still = 30s;
    size_t presses = 0;
    ScreenState text;
    text.name = "text";
    text.matches_observation = [](const ScreenObservation& observation){
        return observation.events.has(ScreenEvent::DIALOG_BOX);
    };
    text.act = [&presses]{
        presses++;
    };
    ScreenState done;
    done.name = "done";
    done.matches_observation = [](const ScreenObservation& observation){
        return observation.events.has(ScreenEvent::PLAYER_CAN_MOVE);
    };
    done.terminal = true;
    const std::vector<ScreenState> states{fade, text, done};
    StepTrace trace("test", [](const std::string&){});
    size_t next = 0;
    const std::function<ScreenObservation()> observe = [&]{
        return looks[std::min(next++, looks.size() - 1)];
    };
    return run_screen_machine_on_observations(observe, scope, states, 600s, trace);
}

UnitTestResult still_states(CancellableScope& scope){
    const WallClock start = current_time();
    std::vector<ScreenObservation> waits;
    for (int second = 0; second < 12; second++){
        waits.push_back(framed_look(start, second, 0xff000000, ScreenEvent::FADE_BLACK));
    }
    waits.push_back(framed_look(start, 12, 0xff808080, ScreenEvent::PLAYER_CAN_MOVE));
    ScreenMachineResult result = run_still_states(waits, scope);
    if (result.terminal != "done"){
        return std::format("expected a 12 s still fade to end on done, got \"{}\"", result.failure);
    }
    std::vector<ScreenObservation> too_long;
    for (int second = 0; second < 40; second++){
        too_long.push_back(framed_look(start, second, 0xff000000, ScreenEvent::FADE_BLACK));
    }
    result = run_still_states(too_long, scope);
    if (result.failure != "The screen stayed still on \"fade\" for more than 30s."){
        return std::format("expected the fade to stay still too long, got \"{}\"", result.failure);
    }
    std::vector<ScreenObservation> presses;
    for (int second = 0; second < 12; second++){
        presses.push_back(framed_look(start, second, 0xffffffff, ScreenEvent::DIALOG_BOX));
    }
    result = run_still_states(presses, scope);
    if (result.failure != "The screen stopped changing: 8 steps on \"text\" did nothing."){
        return std::format("expected the box to stall, got \"{}\"", result.failure);
    }
    return true;
}


//  A named test; it returns its own failure.
struct NamedTest{
    std::string_view name;
    UnitTestResult (*test)(CancellableScope& scope);
};

class Test_ScreenMachine : public UnitTest{
public:
    explicit Test_ScreenMachine(NamedTest test)
        : UnitTest(std::format("CommonTools::ScreenMachine - {}", test.name))
        , m_test(test)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return m_test.test(scope);
    }

private:
    const NamedTest m_test;
};

const NamedTest SCREEN_MACHINE_TESTS[] = {
    {.name = "a run ends on its terminal state", .test = observation_machine_ends},
    {.name = "a run fails: no match, too many visits, timeout", .test = observation_machine_fails},
    {.name = "a waiting state's limit and a stall", .test = still_states},
};


} // namespace


void add_tests_ScreenMachine(UnitTestDatabase& database){
    for (const MatchCase& test : MATCH_CASES){
        database.add<Test_MatchScreenState>(test);
    }
    for (const NamedTest& test : SCREEN_MACHINE_TESTS){
        database.add<Test_ScreenMachine>(test);
    }
}



}  // namespace PokemonAutomation
