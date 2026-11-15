/*  BDSP AutoStory - Launch Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's launch policy driving the shared launch on a scripted console:
 *  each case is the screens the console shows, look by look (the last
 *  repeats), and the presses the launch must make, or where it must fail.
 *  The looks are events and readings only, no frames: nothing is read in
 *  a language. title_screen_shown() is tested apart, on frames.
 *
 */

#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Detect/PokemonBDSP_AutoStory_TitleDetector.h"
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
    BLACK,          //  the boot, the load after the title, or the cut
                    //  from the attract movie back to the title
    QUIET,          //  nothing BDSP's events show: a menu
    //  The title with its red "press A" line: no box or menu, so it shows
    //  PLAYER_CAN_MOVE too, as on the console.
    TITLE,
    //  The title's attract movie, a town with people walking, or a boot
    //  logo: it reads as the overworld.
    MOVIE,
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
    case Screen::TITLE:
        observation.events = ScreenEvents{ScreenEvent::PLAYER_CAN_MOVE};
        observation.readings[std::string(TITLE_READING)] = 1;
        break;
    case Screen::MOVIE:
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
                                        Screen::BLACK, Screen::TITLE, Screen::BLACK})},
     .expectation = {.inputs = "HOME X A A wait ZL wait wait wait"}},
    {.name = "game closed on HOME",
     .input = {.screens = then_settled({Screen::HOME, Screen::HOME, Screen::BLACK, Screen::TITLE, Screen::TITLE,
                                        Screen::BLACK})},
     .expectation = {.inputs = "X A wait ZL ZL wait wait wait"}},
    //  A quiet screen before the launch is the game in play (a menu), not
    //  its title: HOME, to load the save, not ZL.
    {.name = "a quiet screen before the launch is not the title",
     .input = {.screens = then_settled({Screen::QUIET, Screen::HOME, Screen::CLOSE_SOFTWARE, Screen::HOME,
                                        Screen::BLACK, Screen::TITLE, Screen::BLACK})},
     .expectation = {.inputs = "HOME X A A wait ZL wait wait wait"}},
    {.name = "HOME for 1.5 s after the launch, then the boot",
     .input = {.screens = then_settled({Screen::HOME, Screen::HOME, Screen::HOME, Screen::HOME, Screen::HOME,
                                        Screen::BLACK, Screen::TITLE, Screen::BLACK})},
     .expectation = {.inputs = "X A wait wait wait wait ZL wait wait wait"}},
    //  The policy's presses_per_screen ZLs, then the launch gives up.
    {.name = "a title that never loads",
     .input = {.screens = {Screen::HOME, Screen::HOME, Screen::TITLE}},
     .expectation = {.failure = "stuck on title", .inputs = std::format("X A {}", repeated("ZL", 20))}},
    //  The attract movie takes ZL, back to the title, but never counts as
    //  the overworld.
    {.name = "the attract movie is not the overworld",
     .input = {.screens = {Screen::HOME, Screen::HOME, Screen::BLACK, Screen::MOVIE}},
     .expectation = {.failure = "stuck on title", .inputs = std::format("X A wait {}", repeated("ZL", 20))}},
    //  Console B, 2026-11-15 04:21, the game in Japanese: ZL on the movie
    //  went back to the title through a black, which counted as the load,
    //  and the title then settled as the overworld.
    {.name = "a black from the movie back to the title is not the load",
     .input = {.screens = then_settled({Screen::HOME, Screen::HOME, Screen::BLACK, Screen::MOVIE, Screen::BLACK,
                                        Screen::TITLE, Screen::BLACK})},
     .expectation = {.inputs = "X A wait ZL wait ZL wait wait wait"}},
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


//  What a synthetic screen shows in the band under the logo.
enum class Prompt{
    NONE,
    RED_TEXT,   //  red strokes, as letters: about an eighth of the band
    RED_BAR,    //  the band all red, as a button
};

//  title_screen_shown() on a synthetic screen: `background` all over,
//  `corners` in its two bottom corners, `prompt` in the band under the
//  logo.
struct SyntheticTitleCase{
    struct Input{
        uint32_t background;
        uint32_t corners;
        Prompt prompt;
    };
    struct Expectation{
        bool title;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

constexpr uint32_t STARFIELD = 0xff0a2050;
constexpr uint32_t STARFIELD_CORNER = 0xff060a10;
constexpr uint32_t DIALOG_GRAY = 0xff444646;
constexpr uint32_t PROMPT_RED = 0xffd02a2e;

const std::vector<SyntheticTitleCase> SYNTHETIC_TITLE_CASES = {
    {.name = "red line on the starfield",
     .input = {.background = STARFIELD, .corners = STARFIELD_CORNER, .prompt = Prompt::RED_TEXT},
     .expectation = {.title = true}},
    {.name = "starfield before the red line shows",
     .input = {.background = STARFIELD, .corners = STARFIELD_CORNER, .prompt = Prompt::NONE},
     .expectation = {.title = false}},
    {.name = "red text over light corners",
     .input = {.background = STARFIELD, .corners = 0xff808080, .prompt = Prompt::RED_TEXT},
     .expectation = {.title = false}},
    {.name = "a red button on a gray dialog",
     .input = {.background = DIALOG_GRAY, .corners = DIALOG_GRAY, .prompt = Prompt::RED_BAR},
     .expectation = {.title = false}},
    {.name = "a red bar on the starfield",
     .input = {.background = STARFIELD, .corners = STARFIELD_CORNER, .prompt = Prompt::RED_BAR},
     .expectation = {.title = false}},
};

ImageRGB32 synthetic_screen(const SyntheticTitleCase::Input& input){
    const size_t width = 1920;
    const size_t height = 1080;
    ImageRGB32 screen(width, height);
    for (size_t y = 0; y < height; y++){
        for (size_t x = 0; x < width; x++){
            const double fx = (double)x / width;
            const double fy = (double)y / height;
            uint32_t pixel = input.background;
            if (fy >= 0.88 && (fx < 0.10 || fx >= 0.90)){
                pixel = input.corners;
            }
            const bool in_band = fx >= 0.30 && fx < 0.70 && fy >= 0.73 && fy < 0.83;
            const bool stroke = fy >= 0.76 && fy < 0.80 && (x / 6) % 3 == 0;
            if (in_band && (input.prompt == Prompt::RED_BAR || (input.prompt == Prompt::RED_TEXT && stroke))){
                pixel = PROMPT_RED;
            }
            screen.pixel(x, y) = pixel;
        }
    }
    return screen;
}

class SyntheticTitleTest : public UnitTest{
public:
    explicit SyntheticTitleTest(SyntheticTitleCase test)
        : UnitTest(std::format("PokemonBDSP::AutoStory - Title: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const bool title = title_screen_shown(synthetic_screen(m_test.input));
        if (title != m_test.expectation.title){
            return std::format("expected {}", m_test.expectation.title ? "the title" : "not the title");
        }
        return true;
    }

private:
    SyntheticTitleCase m_test;
};


//  title_screen_shown() on a console frame under the test resources;
//  skipped while the frame is not there.
struct FrameTitleCase{
    struct Input{
        std::string_view file;
    };
    struct Expectation{
        bool title;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

const std::vector<FrameTitleCase> FRAME_TITLE_CASES = {
    //  Console B, 2026-11-15 04:21: the launch's failure frame.
    {.name = "Japanese title",
     .input = {.file = "PokemonBDSP/AutoStory/TitleJapanese-20261115.png"},
     .expectation = {.title = true}},
    {.name = "French title",
     .input = {.file = "PokemonBDSP/AutoStory/TitleFrench-20261114.png"},
     .expectation = {.title = true}},
    //  The logo on the starfield, before the red line shows.
    {.name = "Japanese logo before the title",
     .input = {.file = "PokemonBDSP/AutoStory/TitleLogoJapanese-20261115.png"},
     .expectation = {.title = false}},
    //  The player's room, a red rug in the band.
    {.name = "a room's red rug",
     .input = {.file = "PokemonBDSP/AutoStory/RoomRedRug-20261109.png"},
     .expectation = {.title = false}},
    {.name = "Jubilife's Center inside",
     .input = {.file = "PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c6.png"},
     .expectation = {.title = false}},
};

class FrameTitleTest : public UnitTest{
public:
    explicit FrameTitleTest(FrameTitleCase test)
        : UnitTest(std::format("PokemonBDSP::AutoStory - Title frame: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string path = std::format("{}{}", UNIT_TEST_RESOURCE_PATH(), m_test.input.file);
        if (!Filesystem::is_regular_file(path)){
            return UnitTestResult(UnitTestResult::SKIPPED, std::format("Screenshot not found: {}", path));
        }
        const bool title = title_screen_shown(ImageRGB32(path));
        if (title != m_test.expectation.title){
            return std::format("{}: expected {}", m_test.input.file, m_test.expectation.title ? "the title" : "not the title");
        }
        return true;
    }

private:
    FrameTitleCase m_test;
};


} // namespace


void add_tests_AutoStory_Launch(UnitTestDatabase& database){
    for (const LaunchCase& test : LAUNCH_CASES){
        database.add<BdspLaunchTest>(test);
    }
    for (const SyntheticTitleCase& test : SYNTHETIC_TITLE_CASES){
        database.add<SyntheticTitleTest>(test);
    }
    for (const FrameTitleCase& test : FRAME_TITLE_CASES){
        database.add<FrameTitleTest>(test);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
