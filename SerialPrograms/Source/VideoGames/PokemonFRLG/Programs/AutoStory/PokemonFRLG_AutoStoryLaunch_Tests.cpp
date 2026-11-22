/*  AutoStory Launch Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "PokemonFRLG_AutoStoryLaunch.h"
#include "PokemonFRLG_AutoStoryLaunch_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr std::string_view CLOSE_SOFTWARE = "test.close_software";

enum class Screen{
    HOME,
    CLOSE_SOFTWARE,
    CHECK_ONLINE,
    BLACK,
    QUIET,
    LOAD_MENU,
    RECAP,
};

ScreenObservation look(Screen screen){
    ScreenObservation observation;
    switch (screen){
    case Screen::HOME:
        observation.events = ScreenEvents{ScreenEvent::SWITCH_HOME_MENU};
        break;
    case Screen::CLOSE_SOFTWARE:
        observation.readings[std::string(CLOSE_SOFTWARE)] = 1;
        break;
    case Screen::CHECK_ONLINE:
        observation.readings[std::string(FrlgLaunchReadings::CHECK_ONLINE)] = 1;
        break;
    case Screen::BLACK:
        observation.events = ScreenEvents{ScreenEvent::FADE_BLACK};
        break;
    case Screen::QUIET:
        break;
    case Screen::LOAD_MENU:
        observation.readings[std::string(FrlgLaunchReadings::LOAD_MENU)] = 1;
        break;
    case Screen::RECAP:
        observation.events = ScreenEvents{ScreenEvent::DIALOG_BOX};
        break;
    }
    return observation;
}

std::vector<Screen> then_settled(std::vector<Screen> screens){
    screens.insert(screens.end(), {Screen::QUIET, Screen::QUIET, Screen::QUIET, Screen::QUIET});
    return screens;
}

std::vector<Screen> home_looks(size_t count){
    return std::vector<Screen>(count, Screen::HOME);
}

std::vector<Screen> joined(std::vector<Screen> first, const std::vector<Screen>& second){
    first.insert(first.end(), second.begin(), second.end());
    return first;
}

struct LaunchCase{
    struct Input{
        std::vector<Screen> screens = {};
        bool handle_online_license_check = false;
    };
    struct Expectation{
        std::string_view failure = {};
        std::string inputs = {};
    };
    std::string_view name = {};
    Input input = {};
    Expectation expectation = {};
};

std::string a_presses(size_t count){
    std::string text;
    for (size_t c = 0; c < count; c++){
        text += c == 0 ? "A" : " A";
    }
    return text;
}

std::string waits(size_t count){
    std::string text;
    for (size_t c = 0; c < count; c++){
        text += c == 0 ? "wait" : " wait";
    }
    return text;
}

const std::vector<LaunchCase> LAUNCH_CASES = {
    {.name = "reload from the overworld",
     .input = {.screens = then_settled({Screen::QUIET, Screen::HOME, Screen::CLOSE_SOFTWARE, Screen::HOME,
                                        Screen::BLACK, Screen::QUIET, Screen::LOAD_MENU, Screen::BLACK})},
     .expectation = {.inputs = "HOME X A A wait A A wait wait wait wait"}},
    {.name = "game closed on HOME",
     .input = {.screens = then_settled({Screen::HOME, Screen::HOME, Screen::BLACK, Screen::QUIET, Screen::QUIET,
                                        Screen::LOAD_MENU})},
     .expectation = {.inputs = "X A wait A A A wait wait wait"}},
    {.name = "the recap takes B",
     .input = {.screens = then_settled({Screen::HOME, Screen::HOME, Screen::QUIET, Screen::LOAD_MENU, Screen::BLACK,
                                        Screen::RECAP, Screen::RECAP})},
     .expectation = {.inputs = "X A A A wait B B wait wait wait"}},
    {.name = "license check handled",
     .input = {.screens = then_settled({Screen::HOME, Screen::HOME, Screen::CHECK_ONLINE, Screen::HOME,
                                        Screen::QUIET, Screen::LOAD_MENU}),
               .handle_online_license_check = true},
     .expectation = {.inputs = "X A A wait A A wait wait wait"}},
    {.name = "HOME for 1.5 s after the launch, then the boot",
     .input = {.screens = then_settled(joined({Screen::QUIET, Screen::HOME, Screen::CLOSE_SOFTWARE, Screen::HOME},
                                              joined(home_looks(3), {Screen::BLACK, Screen::QUIET, Screen::LOAD_MENU, Screen::BLACK})))},
     .expectation = {.inputs = "HOME X A A wait wait wait wait A A wait wait wait wait"}},
    {.name = "license check's HOME, handled",
     .input = {.screens = then_settled(joined(home_looks(13), {Screen::BLACK, Screen::QUIET, Screen::LOAD_MENU})),
               .handle_online_license_check = true},
     .expectation = {.inputs = "X A " + waits(11) + " wait A A wait wait wait"}},
    {.name = "a title that never leads to the load menu",
     .input = {.screens = {Screen::HOME, Screen::HOME, Screen::QUIET}},
     .expectation = {.failure = "stuck on title", .inputs = "X A " + a_presses(12)}},
};

std::string recorded(std::string_view input){
    if (input == "wait"){
        return "wait 500ms";
    }
    static const std::map<std::string_view, Button> BUTTONS = {
        {"A", BUTTON_A}, {"B", BUTTON_B}, {"X", BUTTON_X}, {"HOME", BUTTON_HOME},
    };
    const auto it = BUTTONS.find(input);
    return it == BUTTONS.end() ? std::format("unknown input {}", input) : std::format("press {}", button_to_string(it->second));
}

std::vector<std::string> expected_inputs(std::string_view inputs){
    std::vector<std::string> expected;
    while (!inputs.empty()){
        const size_t space = inputs.find(' ');
        expected.push_back(recorded(inputs.substr(0, space)));
        inputs = space == std::string_view::npos ? std::string_view() : inputs.substr(space + 1);
    }
    return expected;
}

std::string describe_inputs(const std::vector<std::string>& inputs){
    std::string text;
    for (const std::string& input : inputs){
        text += text.empty() ? input : std::format(", {}", input);
    }
    return std::format("[{}]", text);
}

UnitTestResult check_launch(const LaunchCase& test, CancellableScope& scope){
    std::vector<ScreenObservation> looks;
    for (Screen screen : test.input.screens){
        looks.push_back(look(screen));
    }
    Pokemon::FakeStoryBackend backend(Pokemon::FakeStoryScript{.looks = std::move(looks)}, scope);
    const std::vector<Pokemon::StoryLaunchPrompt> switch_prompts{{
        .name = "close software",
        .matches = [](const ScreenObservation& observation){
            return observation.readings.contains(std::string(CLOSE_SOFTWARE));
        },
    }};
    Pokemon::StoryLaunchPolicy policy = frlg_launch_policy(
        switch_prompts,
        {.handle_online_license_check = test.input.handle_online_license_check}
    );
    policy.tile_selected = [](const ScreenObservation&){
        return true;
    };
    const std::optional<Pokemon::StoryFailure> failed = Pokemon::launch_story_game(backend, policy, {});
    if (test.expectation.failure.empty() && failed){
        return std::format("expected success, got {}", *failed);
    }
    if (!test.expectation.failure.empty()
        && (!failed || failed->code != Pokemon::STORY_LAUNCH_FAILED || failed->detail.find(test.expectation.failure) == std::string::npos)
    ){
        return std::format(
            "expected a launch failure with \"{}\", got {}",
            test.expectation.failure, failed ? std::format("{}", *failed) : std::string("ok")
        );
    }
    const std::vector<std::string> expected = expected_inputs(test.expectation.inputs);
    if (backend.inputs() != expected){
        return std::format("expected inputs {}, got {}", describe_inputs(expected), describe_inputs(backend.inputs()));
    }
    return true;
}

class FrlgLaunchTest : public UnitTest{
public:
    explicit FrlgLaunchTest(LaunchCase test)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Launch: {}", test.name))
        , m_test(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_launch(m_test, scope);
    }

private:
    LaunchCase m_test;
};

} // namespace


void add_tests_AutoStoryLaunch(UnitTestDatabase& database){
    for (const LaunchCase& test : LAUNCH_CASES){
        database.add<FrlgLaunchTest>(test);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
