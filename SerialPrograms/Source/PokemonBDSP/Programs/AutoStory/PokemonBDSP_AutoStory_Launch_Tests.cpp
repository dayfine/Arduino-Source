/*  BDSP AutoStory - Launch Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's launch policy driving the shared launch on a scripted console:
 *  each case is the screens the console shows, look by look (the last
 *  repeats), and the presses the launch must make, or where it must fail.
 *  The looks are events only, no frames: nothing is read in a language.
 *
 */

#include <format>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP_AutoStory_Launch.h"
#include "PokemonBDSP_AutoStory_Launch_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::GridPose;


namespace{


//  The scripted console's "Close software?": the Switch's prompts
//  (Pokemon::switch_launch_prompts()) read a look's frame, and a scripted
//  look has none, so the test answers it from a reading.
constexpr std::string_view CLOSE_SOFTWARE = "test.close_software";

//  What the scripted console shows.
enum class Screen{
    HOME,
    CLOSE_SOFTWARE,
    BLACK,          //  the boot, or the load after the title
    QUIET,          //  nothing BDSP's events show: a logo, the title, a menu
    OVERWORLD,      //  the player in control
};

Pokemon::Observation<GridPose> look(Screen screen){
    Pokemon::Observation<GridPose> observation;
    switch (screen){
    case Screen::HOME:
        observation.events = ScreenEvents{ScreenEvent::SWITCH_HOME_MENU};
        break;
    case Screen::CLOSE_SOFTWARE:
        observation.readings[std::string(CLOSE_SOFTWARE)] = 1;
        break;
    case Screen::BLACK:
        observation.events = ScreenEvents{ScreenEvent::FADE_BLACK};
        break;
    case Screen::QUIET:
        break;
    case Screen::OVERWORLD:
        observation.events = ScreenEvents{ScreenEvent::PLAYER_CAN_MOVE};
        break;
    }
    return observation;
}

//  `screens`, then the overworld for as many looks as it takes to settle.
std::vector<Screen> then_settled(std::vector<Screen> screens){
    screens.insert(screens.end(), {Screen::OVERWORLD, Screen::OVERWORLD, Screen::OVERWORLD});
    return screens;
}

//  One launch.
struct LaunchCase{
    struct Input{
        std::vector<Screen> screens;
    };
    struct Expectation{
        //  Empty: the launch succeeds; else a part of its failure's detail.
        std::string_view failure = {};
        //  The inputs in order, by space: a button ("A", "ZL") or "wait".
        std::string inputs = {};
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

//  `count` of `input`, by space.
std::string repeated(std::string_view input, size_t count){
    std::string text;
    for (size_t c = 0; c < count; c++){
        text += c == 0 ? std::string(input) : std::format(" {}", input);
    }
    return text;
}

const std::vector<LaunchCase> LAUNCH_CASES = {
    {.name = "reload from the overworld",
     .input = {.screens = then_settled({Screen::OVERWORLD, Screen::HOME, Screen::CLOSE_SOFTWARE, Screen::HOME,
                                        Screen::BLACK, Screen::QUIET, Screen::BLACK})},
     .expectation = {.inputs = "HOME X A A wait ZL wait wait wait"}},
    {.name = "game closed on HOME",
     .input = {.screens = then_settled({Screen::HOME, Screen::HOME, Screen::BLACK, Screen::QUIET, Screen::QUIET,
                                        Screen::BLACK})},
     .expectation = {.inputs = "X A wait ZL ZL wait wait wait"}},
    //  A quiet screen before the launch is the game in play (a menu), not
    //  its title: HOME, to load the save, not ZL.
    {.name = "a quiet screen before the launch is not the title",
     .input = {.screens = then_settled({Screen::QUIET, Screen::HOME, Screen::CLOSE_SOFTWARE, Screen::HOME,
                                        Screen::BLACK, Screen::QUIET, Screen::BLACK})},
     .expectation = {.inputs = "HOME X A A wait ZL wait wait wait"}},
    {.name = "HOME for 1.5 s after the launch, then the boot",
     .input = {.screens = then_settled({Screen::HOME, Screen::HOME, Screen::HOME, Screen::HOME, Screen::HOME,
                                        Screen::BLACK, Screen::QUIET, Screen::BLACK})},
     .expectation = {.inputs = "X A wait wait wait wait ZL wait wait wait"}},
    //  The policy's presses_per_screen ZLs, then the launch gives up.
    {.name = "a title that never loads",
     .input = {.screens = {Screen::HOME, Screen::HOME, Screen::QUIET}},
     .expectation = {.failure = "stuck on title", .inputs = std::format("X A {}", repeated("ZL", 20))}},
};

//  The fake backend's record of `input` ("A" -> "press A").
std::string recorded(std::string_view input){
    if (input == "wait"){
        return "wait 500ms";
    }
    static const std::map<std::string_view, Button> BUTTONS = {
        {"A", BUTTON_A}, {"X", BUTTON_X}, {"ZL", BUTTON_ZL}, {"HOME", BUTTON_HOME},
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
    std::vector<Pokemon::Observation<GridPose>> looks;
    for (Screen screen : test.input.screens){
        looks.push_back(look(screen));
    }
    Pokemon::FakeStoryBackend<GridPose> backend({.looks = std::move(looks)}, scope);
    std::vector<Pokemon::StoryLaunchPrompt> switch_prompts{{
        .name = "close software",
        .matches = [](const ScreenObservation& observation){
            return observation.readings.contains(std::string(CLOSE_SOFTWARE));
        },
    }};
    Pokemon::StoryLaunchPolicy policy = bdsp_launch_policy(std::move(switch_prompts));
    //  The scripted HOME has no frame to read a tile name from: its cursor
    //  is on BDSP's tile.
    policy.tile_selected = [](const ScreenObservation&){
        return true;
    };
    const Pokemon::Outcome outcome = Pokemon::launch_story_game(backend, policy, {});
    if (test.expectation.failure.empty() && !outcome.succeeded()){
        return std::format("expected success, got {}", Pokemon::to_string(outcome));
    }
    if (!test.expectation.failure.empty()
        && (outcome.failure != Pokemon::STORY_LAUNCH_FAILED || outcome.detail.find(test.expectation.failure) == std::string::npos)
    ){
        return std::format("expected a launch failure with \"{}\", got {}", test.expectation.failure, Pokemon::to_string(outcome));
    }
    const std::vector<std::string> expected = expected_inputs(test.expectation.inputs);
    if (backend.inputs() != expected){
        return std::format("expected inputs {}, got {}", describe_inputs(expected), describe_inputs(backend.inputs()));
    }
    return true;
}

class BdspLaunchTest : public UnitTest{
public:
    explicit BdspLaunchTest(LaunchCase test)
        : UnitTest(std::format("PokemonBDSP::AutoStory - Launch: {}", test.name))
        , m_test(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_launch(m_test, scope);
    }

private:
    LaunchCase m_test;
};


} // namespace


void add_tests_AutoStory_Launch(UnitTestDatabase& database){
    for (const LaunchCase& test : LAUNCH_CASES){
        database.add<BdspLaunchTest>(test);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
