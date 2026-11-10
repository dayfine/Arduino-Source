/*  PokemonLZA AutoStory - Scene machine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The scene machine's states on sequences of events, as the console
 *  machine reads them: no frames needed.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::ScreenEvent;
using Pokemon::ScreenEvents;

namespace{

//  One read of the screen and what the machine should make of it.
struct Read{
    Milliseconds at;
    ScreenEvents events;
    bool still;
    SceneScreen expected;
};

//  Feeds `reads` to a timeline as the machine does (classify, then record
//  the read it acted on) and checks each state, until a terminal one.
UnitTestResult check_reads(const SceneMachineConfig& config, const std::vector<Read>& reads){
    const WallClock start = current_time();
    SceneTimeline timeline(config, start);
    for (const Read& read : reads){
        const WallClock now = start + read.at;
        const SceneScreen got = timeline.classify(read.events, read.still, now);
        if (got != read.expected){
            return std::format("at {} ms ({}): expected {}, got {}.",
                read.at.count(), Pokemon::describe(read.events),
                scene_screen_name(read.expected), scene_screen_name(got));
        }
        timeline.record(read.events, now);
    }
    return true;
}

SceneMachineConfig opening_config(Milliseconds min_length){
    SceneMachineConfig config;
    config.quiet = 8000ms;
    config.min_length = min_length;
    return config;
}

const ScreenEvents NOTHING{};
const ScreenEvents DIALOG{ScreenEvent::DIALOG_BOX};
const ScreenEvents CHOICE{ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX};
const ScreenEvents PARTY_BAR{ScreenEvent::PLAYER_CAN_MOVE};
const ScreenEvents WHITE{ScreenEvent::FADE_WHITE};
const ScreenEvents BLACK{ScreenEvent::FADE_BLACK};
const ScreenEvents BATTLE{ScreenEvent::BATTLE_STARTED};

//  The opening: a still frame past the quiet time does not end it before
//  the scene's minimum length; then it does.
UnitTestResult test_quiet_waits_for_min_length(Logger&){
    return check_reads(opening_config(60000ms), {
        {0ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {5000ms, DIALOG, false, SceneScreen::DIALOG},
        {10000ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {30000ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {59900ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {60000ms, NOTHING, true, SceneScreen::QUIET_END},
    });
}

//  A box restarts the quiet time; a fade counts as busy too.
UnitTestResult test_quiet_restarts_on_busy(Logger&){
    return check_reads(opening_config(0ms), {
        {0ms, DIALOG, false, SceneScreen::DIALOG},
        {7900ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {7950ms, WHITE, false, SceneScreen::FADE_WHITE},
        {15000ms, WHITE, true, SceneScreen::FADE_WHITE},
        {20000ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {22900ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {23000ms, NOTHING, true, SceneScreen::QUIET_END},
    });
}

//  The party bar ends the scene once it has held a second; a box between
//  lines starts the second again.
UnitTestResult test_party_bar_held(Logger&){
    return check_reads(opening_config(60000ms), {
        {0ms, PARTY_BAR, false, SceneScreen::PARTY_BAR},
        {500ms, PARTY_BAR, true, SceneScreen::PARTY_BAR},
        {600ms, DIALOG, false, SceneScreen::DIALOG},
        {800ms, PARTY_BAR, false, SceneScreen::PARTY_BAR},
        {1700ms, PARTY_BAR, true, SceneScreen::PARTY_BAR},
        {1800ms, PARTY_BAR, true, SceneScreen::CAN_MOVE},
    });
}

//  Without a quiet time only the party bar ends an overworld scene: a
//  still screen waits until the machine's timeout.
UnitTestResult test_no_quiet_never_ends_still(Logger&){
    SceneMachineConfig config;
    return check_reads(config, {
        {0ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {300000ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {300100ms, BLACK, true, SceneScreen::FADE_BLACK},
    });
}

//  Pancham cornered: B through the text and the scene, done at the choice.
UnitTestResult test_text_to_choice(Logger&){
    SceneMachineConfig config;
    config.goal = SceneMashGoal::CHOICE;
    config.text_button = BUTTON_B;
    return check_reads(config, {
        {0ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {1000ms, DIALOG, false, SceneScreen::DIALOG},
        {2000ms, PARTY_BAR, false, SceneScreen::SCENE_MOVING},
        {3000ms, CHOICE, false, SceneScreen::GOAL_CHOICE},
    });
}

//  What each state presses.
UnitTestResult test_state_actions(Logger&){
    SceneMachineConfig a_mash = opening_config(0ms);
    SceneMachineConfig b_to_choice;
    b_to_choice.goal = SceneMashGoal::CHOICE;
    b_to_choice.text_button = BUTTON_B;
    SceneMachineConfig to_battle;
    to_battle.goal = SceneMashGoal::BATTLE;
    struct Case{
        SceneScreen screen;
        const SceneMachineConfig& config;
        SceneMashAction expected;
    };
    const Case cases[] = {
        {SceneScreen::DIALOG, a_mash, SceneMashAction::PRESS_TEXT},
        {SceneScreen::CHOICE, a_mash, SceneMashAction::PRESS_A},
        {SceneScreen::FADE_WHITE, a_mash, SceneMashAction::WAIT},
        {SceneScreen::FADE_BLACK, b_to_choice, SceneMashAction::WAIT},
        {SceneScreen::PARTY_BAR, a_mash, SceneMashAction::WAIT},
        {SceneScreen::SCENE_STILL, a_mash, SceneMashAction::WAIT},
        {SceneScreen::SCENE_MOVING, b_to_choice, SceneMashAction::PRESS_B},
        {SceneScreen::SCENE_MOVING, to_battle, SceneMashAction::PRESS_A},
        {SceneScreen::CAN_MOVE, a_mash, SceneMashAction::DONE},
        {SceneScreen::QUIET_END, a_mash, SceneMashAction::DONE},
        {SceneScreen::GOAL_CHOICE, b_to_choice, SceneMashAction::DONE},
        {SceneScreen::GOAL_BATTLE, to_battle, SceneMashAction::DONE},
    };
    for (const Case& item : cases){
        const SceneMashAction got = scene_screen_action(item.screen, item.config);
        if (got != item.expected){
            return std::format("{}: expected action {}, got {}.",
                scene_screen_name(item.screen), (int)item.expected, (int)got);
        }
    }
    return true;
}

//  A battle goal presses A through cards and stops at the HUD.
UnitTestResult test_to_battle(Logger&){
    SceneMachineConfig config;
    config.goal = SceneMashGoal::BATTLE;
    return check_reads(config, {
        {0ms, DIALOG, false, SceneScreen::DIALOG},
        {1000ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {2000ms, BATTLE, false, SceneScreen::GOAL_BATTLE},
    });
}

//  Every move that ends on an event is a move, and its last leg walks.
UnitTestResult test_move_ends(Logger&){
    if (!story_move_ends().contains("platform_to_plaza")){
        return "Expected platform_to_plaza to end on the white fade.";
    }
    for (const auto& [name, end] : story_move_ends()){
        const auto move = story_moves().find(name);
        if (move == story_moves().end()){
            return name + ": ends on an event but is not a move.";
        }
        if (move->second.legs.empty() || move->second.legs.back().walk <= 0ms){
            return name + ": its last leg has no walk to hold until the event.";
        }
        if (end != ScreenEvent::FADE_WHITE && end != ScreenEvent::FADE_BLACK){
            return name + ": ends on " + Pokemon::screen_event_name(end) + ", not a fade.";
        }
    }
    return true;
}


struct SceneMachineCase{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

class Test_AutoStorySceneMachine : public UnitTest{
public:
    explicit Test_AutoStorySceneMachine(SceneMachineCase test_case)
        : UnitTest("PokemonLZA::AutoStorySceneMachine - " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return m_case.body(logger);
    }

private:
    SceneMachineCase m_case;
};

} // namespace


void add_tests_AutoStorySceneMachine(UnitTestDatabase& database){
    const SceneMachineCase cases[] = {
        {"quiet end waits for the scene's minimum length", test_quiet_waits_for_min_length},
        {"a box or a fade restarts the quiet time", test_quiet_restarts_on_busy},
        {"the party bar held a second ends the scene", test_party_bar_held},
        {"without a quiet time a still screen never ends it", test_no_quiet_never_ends_still},
        {"B through the text to the choice", test_text_to_choice},
        {"A through the cards to the battle HUD", test_to_battle},
        {"what each state presses", test_state_actions},
        {"walks that end on a fade", test_move_ends},
    };
    for (const SceneMachineCase& item : cases){
        database.add<Test_AutoStorySceneMachine>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
