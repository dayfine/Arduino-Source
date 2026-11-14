/*  Story Launch Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The launch on a scripted console: each case is the screens the console
 *  shows, look by look (the last repeats), and the presses the launch must
 *  make, or where it must fail. The test game's screens are readings its
 *  policy reads; HOME is the shared SWITCH_HOME_MENU event and the
 *  overworld the shared PLAYER_CAN_MOVE.
 *
 */

#include <format>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryLaunch.h"
#include "Pokemon_StoryLaunch_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

using namespace NintendoSwitch;


namespace{


//  What the scripted console shows.
enum class Screen{
    HOME_ON_GAME,       //  HOME, the cursor on the game's tile
    HOME_OFF_GAME,      //  HOME, the cursor on another tile
    CLOSE_DIALOG,       //  HOME's "Close software?"
    LICENSE_CHECK,      //  the online license check
    BLACK,              //  a boot or a load
    TITLE,
    NEW_GAME,           //  the title's menu without a save
    BACKUP_CONFIRM,     //  "Load the backup save?"
    LOAD_MENU,          //  Continue / New Game
    UPDATE,             //  the update prompt: Up twice to its last choice, then A
    GAME_DIALOG,        //  the game's dialog box, in play or on the way
    OVERWORLD,
};

inline constexpr std::string_view SCREEN = "screen";
inline constexpr std::string_view TILE = "home.tile";

Observation<GridPose> look(Screen screen){
    Observation<GridPose> observation;
    switch (screen){
    case Screen::HOME_ON_GAME:
    case Screen::HOME_OFF_GAME:
        observation.events = ScreenEvents{ScreenEvent::SWITCH_HOME_MENU};
        observation.readings[std::string(TILE)] = screen == Screen::HOME_ON_GAME ? 1 : 0;
        return observation;
    case Screen::OVERWORLD:
        observation.events = ScreenEvents{ScreenEvent::PLAYER_CAN_MOVE};
        return observation;
    default:
        observation.readings[std::string(SCREEN)] = (int)screen;
        return observation;
    }
}

std::function<bool(const ScreenObservation&)> shows(Screen screen){
    return [screen](const ScreenObservation& observation){
        const auto it = observation.readings.find(SCREEN);
        return it != observation.readings.end() && it->second == (int)screen;
    };
}

//  Whether the test game has a backup save.
enum class Backup{
    PLZA,   //  Up, then B + X
    NONE,
};

//  How the test game finds its tile on HOME.
enum class TileCheck{
    READINGS,       //  the scripted "home.tile" reading
    SWITCH_NAME,    //  switch_home_tile_named(), which reads the look's frame
};

//  How loose the test game's title and dialog checks are.
enum class GameScreens{
    STRICT,         //  read whenever they show
    AFTER_LAUNCH,   //  read only once the launch launched the game
};

//  The test game: A on the title, `backup` for the backup save, A on each
//  prompt (Up, Up, A on the update prompt; B on the game's dialog), and
//  small bounds so a failure takes few looks.
StoryLaunchPolicy test_policy(Backup backup, GameScreens game_screens, TileCheck tile_check){
    StoryLaunchPolicy policy;
    policy.game = "test";
    policy.tile_selected = [](const ScreenObservation& observation){
        const auto it = observation.readings.find(TILE);
        return it != observation.readings.end() && it->second == 1;
    };
    if (tile_check == TileCheck::SWITCH_NAME){
        policy.tile_selected = switch_home_tile_named(Language::English, "Pokémon Legends: Z-A");
    }
    policy.tile_search = 2;
    policy.title = shows(Screen::TITLE);
    if (backup == Backup::PLZA){
        policy.backup_save = {{.button = BUTTON_UP}, {.button = BUTTON_B | BUTTON_X, .hold = Milliseconds(1000)}};
    }
    policy.new_game = shows(Screen::NEW_GAME);
    policy.prompts = {
        {.name = "close software", .matches = shows(Screen::CLOSE_DIALOG)},
        {.name = "license check", .matches = shows(Screen::LICENSE_CHECK)},
        {.name = "backup confirm", .matches = shows(Screen::BACKUP_CONFIRM)},
        {.name = "load menu", .matches = shows(Screen::LOAD_MENU)},
        {.name = "update", .matches = shows(Screen::UPDATE), .before = {{.button = BUTTON_UP}, {.button = BUTTON_UP}}},
        {.name = "game dialog", .matches = shows(Screen::GAME_DIALOG), .press = {.button = BUTTON_B},
         .after_launch = game_screens == GameScreens::AFTER_LAUNCH},
    };
    policy.title_after_launch = game_screens == GameScreens::AFTER_LAUNCH;
    policy.presses_per_screen = 3;
    policy.launch_wait = Milliseconds(2000);
    policy.max_looks = 12;
    return policy;
}

//  `screens`, then the overworld for as many looks as it takes to settle.
std::vector<Screen> then_settled(std::vector<Screen> screens){
    screens.insert(screens.end(), {Screen::OVERWORLD, Screen::OVERWORLD, Screen::OVERWORLD});
    return screens;
}

using Save = StoryLaunchRequest::Save;
using Running = StoryLaunchRequest::Running;

//  One launch.
struct LaunchCase{
    struct Input{
        std::vector<Screen> screens;
        StoryLaunchRequest request = {};
        Backup backup = Backup::PLZA;
        GameScreens game_screens = GameScreens::STRICT;
        TileCheck tile_check = TileCheck::READINGS;
    };
    struct Expectation{
        //  Empty: the launch succeeds; else a part of its failure's detail.
        std::string_view failure = {};
        //  The inputs in order, by space: a button ("A", "HOME", "B+X")
        //  or "wait".
        std::string_view inputs = {};
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

const std::vector<LaunchCase> LAUNCH_CASES = {
    {.name = "game closed on HOME",
     .input = {.screens = then_settled({Screen::HOME_ON_GAME, Screen::HOME_ON_GAME, Screen::BLACK, Screen::TITLE, Screen::BLACK})},
     .expectation = {.inputs = "X A wait A wait wait wait"}},
    {.name = "game open on its title",
     .input = {.screens = then_settled({Screen::TITLE, Screen::BLACK})},
     .expectation = {.inputs = "A wait wait wait"}},
    {.name = "game on the overworld kept",
     .input = {.screens = then_settled({}), .request = {.running = Running::KEEP}},
     .expectation = {.inputs = "wait wait"}},
    {.name = "game on the overworld restarted",
     .input = {.screens = then_settled({Screen::OVERWORLD, Screen::HOME_ON_GAME, Screen::CLOSE_DIALOG, Screen::HOME_ON_GAME,
                                        Screen::BLACK, Screen::TITLE})},
     .expectation = {.inputs = "HOME X A A wait A wait wait"}},
    {.name = "backup save from the overworld",
     .input = {.screens = then_settled({Screen::OVERWORLD, Screen::HOME_ON_GAME, Screen::HOME_ON_GAME, Screen::TITLE,
                                        Screen::BACKUP_CONFIRM, Screen::BLACK}),
               .request = {.save = Save::BACKUP, .running = Running::KEEP}},
     .expectation = {.inputs = "HOME X A Up B+X A A wait wait wait"}},
    {.name = "backup save the game does not have",
     .input = {.screens = then_settled({}), .request = {.save = Save::BACKUP}, .backup = Backup::NONE},
     .expectation = {.failure = "no backup save", .inputs = ""}},
    {.name = "license check back to HOME",
     .input = {.screens = then_settled({Screen::HOME_ON_GAME, Screen::HOME_ON_GAME, Screen::LICENSE_CHECK, Screen::HOME_ON_GAME,
                                        Screen::BLACK, Screen::TITLE, Screen::LOAD_MENU})},
     .expectation = {.inputs = "X A A wait wait A A wait wait"}},
    //  Console B, 2026-11-14 02:41 (FRLG): HOME showed for about 1.5 s
    //  after the A on the tile, and each look there pressed A again.
    {.name = "HOME after the launch press waits",
     .input = {.screens = then_settled({Screen::HOME_ON_GAME, Screen::HOME_ON_GAME, Screen::HOME_ON_GAME, Screen::HOME_ON_GAME,
                                        Screen::HOME_ON_GAME, Screen::BLACK, Screen::TITLE})},
     .expectation = {.inputs = "X A wait wait wait wait A wait wait"}},
    {.name = "HOME past the launch wait takes A again",
     .input = {.screens = then_settled({Screen::HOME_ON_GAME, Screen::HOME_ON_GAME, Screen::HOME_ON_GAME, Screen::HOME_ON_GAME,
                                        Screen::HOME_ON_GAME, Screen::HOME_ON_GAME, Screen::HOME_ON_GAME, Screen::TITLE})},
     .expectation = {.inputs = "X A wait wait wait wait A A wait wait"}},
    {.name = "cursor on another tile",
     .input = {.screens = then_settled({Screen::HOME_OFF_GAME, Screen::HOME_OFF_GAME, Screen::HOME_ON_GAME}),
               .request = {.running = Running::KEEP}},
     .expectation = {.inputs = "Left Left A wait wait"}},
    {.name = "game not on HOME",
     .input = {.screens = {Screen::HOME_OFF_GAME}},
     .expectation = {.failure = "tile is not on HOME", .inputs = "Left Left Right Right Right Right"}},
    {.name = "Switch tile check never launches without a frame",
     .input = {.screens = {Screen::HOME_ON_GAME}, .tile_check = TileCheck::SWITCH_NAME},
     .expectation = {.failure = "tile is not on HOME", .inputs = "Left Left Right Right Right Right"}},
    {.name = "title ignores A",
     .input = {.screens = {Screen::TITLE}},
     .expectation = {.failure = "stuck on title", .inputs = "A A A"}},
    {.name = "no save never starts a new game",
     .input = {.screens = {Screen::NEW_GAME}},
     .expectation = {.failure = "new game", .inputs = ""}},
    {.name = "black screen after launch times out",
     .input = {.screens = {Screen::HOME_ON_GAME, Screen::HOME_ON_GAME, Screen::BLACK}},
     .expectation = {.failure = "no overworld after 12 looks", .inputs = "X A wait wait wait wait wait wait wait wait wait wait"}},
    {.name = "kept game off the overworld takes B",
     .input = {.screens = then_settled({Screen::LOAD_MENU, Screen::BLACK}), .request = {.running = Running::KEEP}},
     .expectation = {.inputs = "A B wait wait"}},
    {.name = "update prompt takes its lead-in presses",
     .input = {.screens = then_settled({Screen::HOME_ON_GAME, Screen::HOME_ON_GAME, Screen::UPDATE, Screen::BLACK, Screen::TITLE})},
     .expectation = {.inputs = "X A Up Up A wait A wait wait"}},
    {.name = "strict title in front is pressed",
     .input = {.screens = then_settled({Screen::GAME_DIALOG, Screen::TITLE, Screen::BLACK})},
     .expectation = {.inputs = "B A wait wait wait"}},
    {.name = "loose title in front is left with HOME",
     .input = {.screens = then_settled({Screen::GAME_DIALOG, Screen::TITLE, Screen::HOME_ON_GAME, Screen::HOME_ON_GAME,
                                        Screen::BLACK, Screen::GAME_DIALOG, Screen::TITLE}),
               .game_screens = GameScreens::AFTER_LAUNCH},
     .expectation = {.inputs = "HOME HOME X A wait B A wait wait"}},
};

//  The fake backend's record of `input` ("A" -> "press A").
std::string recorded(std::string_view input){
    if (input == "wait"){
        return "wait 500ms";
    }
    static const std::map<std::string_view, Button> BUTTONS = {
        {"A", BUTTON_A}, {"B", BUTTON_B}, {"X", BUTTON_X}, {"HOME", BUTTON_HOME},
        {"Up", BUTTON_UP}, {"Left", BUTTON_LEFT}, {"Right", BUTTON_RIGHT}, {"B+X", BUTTON_B | BUTTON_X},
    };
    const auto it = BUTTONS.find(input);
    return it == BUTTONS.end() ? std::format("unknown input {}", input) : std::format("press {}", button_to_string(it->second));
}

//  `inputs` (by space) as the fake backend records them.
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

std::string check_launch(const LaunchCase& test, CancellableScope& scope){
    std::vector<Observation<GridPose>> looks;
    for (Screen screen : test.input.screens){
        looks.push_back(look(screen));
    }
    FakeStoryBackend<GridPose> backend({.looks = std::move(looks)}, scope);
    const Outcome outcome = launch_story_game(backend, test_policy(test.input.backup, test.input.game_screens, test.input.tile_check), test.input.request);
    if (test.expectation.failure.empty() && !outcome.succeeded()){
        return std::format("expected success, got {}", to_string(outcome));
    }
    if (!test.expectation.failure.empty()){
        if (outcome.failure != STORY_LAUNCH_FAILED || outcome.detail.find(test.expectation.failure) == std::string::npos){
            return std::format("expected a launch failure with \"{}\", got {}", test.expectation.failure, to_string(outcome));
        }
    }
    const std::vector<std::string> expected = expected_inputs(test.expectation.inputs);
    if (backend.inputs() != expected){
        return std::format("expected inputs {}, got {}", describe_inputs(expected), describe_inputs(backend.inputs()));
    }
    return "";
}

class StoryLaunchTest : public UnitTest{
public:
    explicit StoryLaunchTest(LaunchCase test)
        : UnitTest(std::format("Pokemon::StoryLaunch - {}", test.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = check_launch(m_case, scope);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    LaunchCase m_case;
};


} // namespace


void add_tests_StoryLaunch(UnitTestDatabase& database){
    for (const LaunchCase& test : LAUNCH_CASES){
        database.add<StoryLaunchTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
