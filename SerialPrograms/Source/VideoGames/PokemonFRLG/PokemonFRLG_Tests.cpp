/*  Pokemon FRLG Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <fstream>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/StaticGlobals.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryScreenEvents.h"
#include "Inference/Box/PokemonFRLG_BoxCellOccupiedDetector.h"
#include "Inference/Box/PokemonFRLG_BoxCellReader.h"
#include "Inference/Box/PokemonFRLG_BoxGenderReader.h"
#include "Inference/Box/PokemonFRLG_BoxSelectionModeDetector.h"
#include "Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "Inference/Dialogs/PokemonFRLG_PrizeSelectDetector.h"
#include "Inference/Map/PokemonFRLG_PokemonLeagueDetectors.h"
#include "Inference/Menus/PokemonFRLG_BagDetector.h"
#include "Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "Inference/Menus/PokemonFRLG_OptionMenuReader.h"
#include "Inference/Menus/PokemonFRLG_PartyEmptySlotDetector.h"
#include "Inference/Menus/PokemonFRLG_PartyHeldItemDetector.h"
#include "Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "Inference/Menus/PokemonFRLG_SaveCardReader.h"
#include "Inference/Menus/PokemonFRLG_SummaryMovesReader.h"
#include "Inference/PokemonFRLG_BattleLevelUpReader.h"
#include "Inference/PokemonFRLG_BattleSelectionArrowDetector.h"
#include "Inference/PokemonFRLG_PartyLevelUpReader.h"
#include "Inference/PokemonFRLG_ShinySymbolDetector.h"
#include "Inference/PokemonFRLG_StatsReader.h"
#include "Inference/PokemonFRLG_TrainerIdReader.h"
#include "Inference/PokemonFRLG_WildEncounterReader.h"
#include "Overworld/PokemonFRLG_OverworldMaps.h"
#include "Overworld/PokemonFRLG_OverworldReanchor.h"
#include "Overworld/PokemonFRLG_OverworldRecovery.h"
#include "Overworld/PokemonFRLG_OverworldRoute.h"
#include "Overworld/PokemonFRLG_OverworldWalk.h"
#include "Overworld/PokemonFRLG_PlayerFacingReader.h"
#include "Programs/PokemonFRLG_NamingScreenKeyboard.h"
#include "PokemonFRLG_Navigation.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryBattle.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryRngLoop.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryStepTable.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryTable.h"
#include "Programs/AutoStory/PokemonFRLG_AutoStoryExport_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngPlanInputs_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngPlanLog_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngPlanTracker_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngPlan_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngSteps_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_RngTargetSearch_Tests.h"
#include "Programs/RngManipulation/PokemonFRLG_StarterRngLoop_Tests.h"
#include "Tests/TestUtils.h"

#include "PokemonFRLG_Tests.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class NullLogger : public Logger{
public:
    virtual void log(const std::string&, Color) override{}
};

std::string button_name(Button button){
    return button == BUTTON_A ? "A" : button == BUTTON_B ? "B" : "?";
}

// Steps that return scripted events, and record what exit_wild_battle() asked for.
class ScriptedWildBattleExit : public WildBattleExitActions{
public:
    // Returned by the press steps in turn; None once used up. Mashing B only watches for the
    // battle to end, so it takes the next event only if that is BattleExited.
    std::vector<WildBattleExitEvent> events;
    // Returned by evolution_started() in turn; false once used up.
    std::vector<bool> evolutions;
    // How many overworld checks fail before one finds it.
    int overworld_misses = 0;

    std::vector<std::string> steps;
    // Seconds spent in steps that ran to their timeout, and waiting for an evolution: time in
    // which the player sees nothing happen.
    int idle_seconds = 0;

    virtual WildBattleExitEvent mash_b_until_battle_exited() override{
        steps.push_back("mash-B");
        if (m_next_event < events.size() && events[m_next_event] == WildBattleExitEvent::BattleExited){
            return events[m_next_event++];
        }
        idle_seconds += 20;
        return WildBattleExitEvent::None;
    }
    virtual WildBattleExitEvent press_until_dialog(Button button) override{
        steps.push_back("press-" + button_name(button));
        WildBattleExitEvent event = next_event();
        if (event == WildBattleExitEvent::None){
            idle_seconds += 30;
        }
        return event;
    }
    virtual bool evolution_started() override{
        steps.push_back("evolution?");
        return m_next_evolution < evolutions.size() && evolutions[m_next_evolution++];
    }
    virtual void wait_for_evolution() override{
        steps.push_back("wait");
        idle_seconds += 20;
    }
    virtual void advance_dialog() override{
        steps.push_back("advance");
    }
    virtual void answer_move_prompt(Button button) override{
        steps.push_back("answer-" + button_name(button));
    }
    virtual bool return_to_overworld() override{
        steps.push_back("overworld?");
        return overworld_misses-- <= 0;
    }

private:
    WildBattleExitEvent next_event(){
        return m_next_event < events.size() ? events[m_next_event++] : WildBattleExitEvent::None;
    }

    size_t m_next_event = 0;
    size_t m_next_evolution = 0;
};

struct WildBattleExitScenario{
    std::string name;
    std::vector<WildBattleExitEvent> events = {};
    std::vector<bool> evolutions = {};
    int overworld_misses = 0;
    bool stop_on_move_learn = false;
    bool prevent_evolution = false;
    WildBattleExitResult expected_result = WildBattleExitResult::Exited;
    std::string expected_steps;
    int expected_idle_seconds = 0;
};

std::string result_name(WildBattleExitResult result){
    switch (result){
    case WildBattleExitResult::Exited:                 return "Exited";
    case WildBattleExitResult::ExitedAfterMoveLearn:   return "ExitedAfterMoveLearn";
    case WildBattleExitResult::StoppedOnMoveLearn:     return "StoppedOnMoveLearn";
    case WildBattleExitResult::Failed:                 return "Failed";
    }
    return "?";
}

class Test_ExitWildBattle : public UnitTest{
public:
    Test_ExitWildBattle(WildBattleExitScenario scenario)
        : UnitTest("PokemonFRLG::ExitWildBattle - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        ScriptedWildBattleExit actions;
        actions.events = m_scenario.events;
        actions.evolutions = m_scenario.evolutions;
        actions.overworld_misses = m_scenario.overworld_misses;

        NullLogger logger;
        const WildBattleExitResult result = exit_wild_battle(
            logger, actions, m_scenario.stop_on_move_learn, m_scenario.prevent_evolution
        );
        if (result != m_scenario.expected_result){
            return "Expected " + result_name(m_scenario.expected_result) + ", got " + result_name(result);
        }
        std::string steps;
        for (const std::string& step : actions.steps){
            if (!steps.empty()){
                steps += ' ';
            }
            steps += step;
        }
        if (steps != m_scenario.expected_steps){
            return "Expected steps: " + m_scenario.expected_steps + "\nActual steps:   " + steps;
        }
        if (actions.idle_seconds != m_scenario.expected_idle_seconds){
            return "Expected " + std::to_string(m_scenario.expected_idle_seconds) + " idle seconds, got "
                + std::to_string(actions.idle_seconds);
        }
        return true;
    }

private:
    const WildBattleExitScenario m_scenario;
};

void add_tests_ExitWildBattle(UnitTestDatabase& database){
    using enum WildBattleExitEvent;
    const WildBattleExitScenario scenarios[] = {
        {
            .name = "battle exits",
            .events = {BattleExited},
            .expected_steps = "mash-B evolution?",
        },
        {
            // Mashing B did not end the battle, so it presses B until a dialog shows.
            .name = "dialog is advanced",
            .events = {AdvanceDialog, BattleExited},
            .expected_steps = "mash-B press-B advance press-B evolution?",
            .expected_idle_seconds = 20,
        },
        {
            // Decline "forget a move?" with B, then confirm "stop learning?" with A.
            .name = "new move is declined",
            .events = {MoveLearnPrompt, MoveLearnPrompt, BattleExited},
            .expected_result = WildBattleExitResult::ExitedAfterMoveLearn,
            .expected_steps = "mash-B press-B answer-B press-A answer-A press-A evolution?",
            .expected_idle_seconds = 20,
        },
        {
            .name = "stops at a new move when asked",
            .events = {MoveLearnPrompt},
            .stop_on_move_learn = true,
            .expected_result = WildBattleExitResult::StoppedOnMoveLearn,
            .expected_steps = "mash-B press-B",
            .expected_idle_seconds = 20,
        },
        {
            // After an evolution nothing ends the battle again, so the overworld is checked
            // right after mashing B, without pressing B until the deadline first.
            .name = "evolution",
            .events = {BattleExited},
            .evolutions = {true},
            .expected_steps = "mash-B evolution? wait mash-B overworld?",
            .expected_idle_seconds = 40,
        },
        {
            // Mashing B right away cancels the evolution.
            .name = "prevented evolution",
            .events = {BattleExited},
            .evolutions = {true},
            .prevent_evolution = true,
            .expected_steps = "mash-B evolution? mash-B overworld?",
            .expected_idle_seconds = 20,
        },
        {
            // A move to learn after the evolution keeps the overworld from being found, so it
            // goes back to pressing B, which finds the move-learn prompt.
            .name = "evolution then new move is declined",
            .events = {BattleExited, MoveLearnPrompt, MoveLearnPrompt},
            .evolutions = {true},
            .overworld_misses = 1,
            .expected_result = WildBattleExitResult::ExitedAfterMoveLearn,
            .expected_steps =
                "mash-B evolution? wait mash-B overworld? "
                "press-B answer-B press-A answer-A press-A overworld?",
            .expected_idle_seconds = 70,
        },
        {
            // If the overworld is not found after the evolution, it goes back to pressing B.
            .name = "evolution and overworld not found",
            .events = {BattleExited},
            .evolutions = {true},
            .overworld_misses = 100,
            .expected_result = WildBattleExitResult::Failed,
            .expected_steps =
                "mash-B evolution? wait mash-B overworld? "
                "press-B overworld? press-B overworld? press-B overworld? "
                "press-B overworld? press-B overworld? press-B overworld?",
            .expected_idle_seconds = 220,
        },
        {
            .name = "gives up when the overworld is not found",
            .overworld_misses = 100,
            .expected_result = WildBattleExitResult::Failed,
            .expected_steps =
                "mash-B press-B overworld? press-B overworld? press-B overworld? "
                "press-B overworld? press-B overworld? press-B overworld?",
            .expected_idle_seconds = 200,
        },
    };
    for (const WildBattleExitScenario& scenario : scenarios){
        database.add<Test_ExitWildBattle>(scenario);
    }
}


//  Naming screen keyboard: cursor moves, plans and text checks, against pokefirered's
//  src/naming_screen.c.

struct NamingCursorInput{
    std::string name = "";
    NamingCursor start = {};
    std::vector<NamingAction> actions = {};
};

struct NamingCursorExpectation{
    NamingCursor expected = {};
};

struct NamingCursorScenario{
    NamingCursorInput input;
    NamingCursorExpectation expectation;
};

class Test_NamingCursor : public UnitTest{
public:
    explicit Test_NamingCursor(NamingCursorScenario scenario)
        : UnitTest("PokemonFRLG::NamingScreen cursor - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        NamingCursor cursor = m_scenario.input.start;
        for (NamingAction action : m_scenario.input.actions){
            cursor = apply_naming_action(cursor, action);
        }
        if (cursor != m_scenario.expectation.expected){
            return "After " + to_string(m_scenario.input.actions) + ": expected " + to_string(m_scenario.expectation.expected)
                + ", got " + to_string(cursor);
        }
        return true;
    }

private:
    const NamingCursorScenario m_scenario;
};

const NamingCursorScenario NAMING_CURSOR_CASES[] = {
    //  Crossing and moving on the button column.
    {
        .input = {
            .name = "left from A goes to the page button",
            .actions = {NamingAction::LEFT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 0, .column = 8, .remembered_row = 1},
        },
    },
    {
        .input = {
            .name = "left and right return to A",
            .actions = {NamingAction::LEFT, NamingAction::RIGHT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 0, .column = 0, .remembered_row = 1},
        },
    },
    {
        .input = {
            .name = "row 1 crosses the button column",
            .start = {.page = NamingScreenPage::UPPER, .row = 1, .column = 0},
            .actions = {NamingAction::LEFT, NamingAction::RIGHT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 1, .column = 0, .remembered_row = 1},
        },
    },
    {
        .input = {
            .name = "row 2 crosses the button column",
            .start = {.page = NamingScreenPage::UPPER, .row = 2, .column = 0},
            .actions = {NamingAction::LEFT, NamingAction::RIGHT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 2, .column = 0, .remembered_row = 2},
        },
    },
    {
        .input = {
            .name = "row 3 crosses the OK button",
            .start = {.page = NamingScreenPage::UPPER, .row = 3, .column = 0},
            .actions = {NamingAction::LEFT, NamingAction::RIGHT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 3, .column = 0, .remembered_row = 2},
        },
    },
    {
        .input = {
            .name = "right from the last column wraps through the buttons",
            .start = {.page = NamingScreenPage::UPPER, .row = 0, .column = 7},
            .actions = {NamingAction::RIGHT, NamingAction::RIGHT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 0, .column = 0, .remembered_row = 1},
        },
    },
    {
        .input = {
            .name = "moving on the button column forgets row 2",
            .start = {.page = NamingScreenPage::UPPER, .row = 2, .column = 0},
            .actions = {NamingAction::LEFT, NamingAction::UP, NamingAction::DOWN, NamingAction::RIGHT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 1, .column = 0, .remembered_row = 1},
        },
    },
    //  Wrap-around on the keys and the button column.
    {
        .input = {
            .name = "button column wraps over three buttons",
            .start = {.page = NamingScreenPage::UPPER, .row = 0, .column = 8, .remembered_row = 1},
            .actions = {NamingAction::UP},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 2, .column = 8, .remembered_row = 2},
        },
    },
    {
        .input = {
            .name = "rows wrap up",
            .actions = {NamingAction::UP},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 3, .column = 0},
        },
    },
    {
        .input = {
            .name = "rows wrap down",
            .start = {.page = NamingScreenPage::UPPER, .row = 3, .column = 0},
            .actions = {NamingAction::DOWN},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 0, .column = 0},
        },
    },
    {
        .input = {
            .name = "symbols page has six key columns",
            .start = {.page = NamingScreenPage::SYMBOLS, .row = 0, .column = 0},
            .actions = {NamingAction::LEFT, NamingAction::LEFT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::SYMBOLS, .row = 0, .column = 5, .remembered_row = 1},
        },
    },
    //  SELECT and START.
    {
        .input = {
            .name = "SELECT goes to lower case",
            .actions = {NamingAction::SELECT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::LOWER, .row = 0, .column = 0},
        },
    },
    {
        .input = {
            .name = "SELECT twice goes to symbols",
            .actions = {NamingAction::SELECT, NamingAction::SELECT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::SYMBOLS, .row = 0, .column = 0},
        },
    },
    {
        .input = {
            .name = "SELECT three times returns to upper case",
            .actions = {NamingAction::SELECT, NamingAction::SELECT, NamingAction::SELECT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 0, .column = 0},
        },
    },
    {
        .input = {
            .name = "SELECT moves the cursor into the symbols columns",
            .start = {.page = NamingScreenPage::UPPER, .row = 0, .column = 7},
            .actions = {NamingAction::SELECT, NamingAction::SELECT, NamingAction::SELECT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::UPPER, .row = 0, .column = 5},
        },
    },
    {
        .input = {
            .name = "SELECT keeps the cursor on the button column",
            .start = {.page = NamingScreenPage::UPPER, .row = 1, .column = 8, .remembered_row = 1},
            .actions = {NamingAction::SELECT, NamingAction::SELECT},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::SYMBOLS, .row = 1, .column = 6, .remembered_row = 1},
        },
    },
    {
        .input = {
            .name = "START goes to OK",
            .start = {.page = NamingScreenPage::LOWER, .row = 1, .column = 3},
            .actions = {NamingAction::START},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::LOWER, .row = 2, .column = 8},
        },
    },
    {
        .input = {
            .name = "START goes to OK on symbols",
            .start = {.page = NamingScreenPage::SYMBOLS, .row = 0, .column = 0},
            .actions = {NamingAction::START},
        },
        .expectation = {
            .expected = {.page = NamingScreenPage::SYMBOLS, .row = 2, .column = 6},
        },
    },
};

void add_tests_NamingCursor(UnitTestDatabase& database){
    for (const NamingCursorScenario& scenario : NAMING_CURSOR_CASES){
        database.add<Test_NamingCursor>(scenario);
    }
}


struct NamingPlanInput{
    std::string name = "";
    std::string text = "";
    NamingScreenKind kind = NamingScreenKind::PLAYER;
};

struct NamingPlanExpectation{
    //  What the game shows when it differs from `text` (another spelling of a key).
    std::string typed = "";
    //  The exact plan, when the shortest one is unique; "" to skip.
    std::string expected_actions = "";
    //  The number of presses, including A and the final START A; 0 to skip.
    size_t expected_presses = 0;
};

struct NamingPlanScenario{
    NamingPlanInput input;
    NamingPlanExpectation expectation;
};

std::string replay_naming_plan(
    const std::vector<NamingAction>& actions,
    const std::string& plan,
    const NamingPlanScenario& scenario
){
    std::string typed;
    NamingCursor cursor;
    for (size_t i = 0; i < actions.size(); i++){
        cursor = apply_naming_action(cursor, actions[i]);
        if (actions[i] != NamingAction::A){
            continue;
        }
        const uint8_t columns = NAMING_SCREEN_KEY_COLUMNS[(size_t)cursor.page];
        if (i + 1 == actions.size()){
            if (cursor.column != columns || cursor.row != NAMING_SCREEN_BUTTON_OK){
                return "The last A is not on OK but on " + to_string(cursor) + ": " + plan;
            }
            break;
        }
        if (cursor.column >= columns){
            return "A pressed on the button column at " + to_string(cursor) + ": " + plan;
        }
        typed += NAMING_SCREEN_KEYS[(size_t)cursor.page][cursor.row][cursor.column];
    }
    const std::string& expected = scenario.expectation.typed.empty() ? scenario.input.text : scenario.expectation.typed;
    if (typed != expected){
        return "Expected the plan to type \"" + expected + "\", it types \"" + typed + "\": " + plan;
    }
    return "";
}

class Test_NamingPlan : public UnitTest{
public:
    explicit Test_NamingPlan(NamingPlanScenario scenario)
        : UnitTest("PokemonFRLG::NamingScreen plan - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<NamingAction> actions = plan_naming_input(m_scenario.input.text, m_scenario.input.kind);
        const std::string plan = to_string(actions);
        if (!m_scenario.expectation.expected_actions.empty() && plan != m_scenario.expectation.expected_actions){
            return "Expected plan: " + m_scenario.expectation.expected_actions + "\nActual plan:   " + plan;
        }
        if (m_scenario.expectation.expected_presses != 0 && actions.size() != m_scenario.expectation.expected_presses){
            return "Expected " + std::to_string(m_scenario.expectation.expected_presses) + " presses, got "
                + std::to_string(actions.size()) + ": " + plan;
        }
        std::string replay_error = replay_naming_plan(actions, plan, m_scenario);
        if (!replay_error.empty()){
            return replay_error;
        }
        return true;
    }

private:
    const NamingPlanScenario m_scenario;
};

const NamingPlanScenario NAMING_PLAN_CASES[] = {
    {
        .input = {.name = "starts on A", .text = "A"},
        .expectation = {.expected_actions = "A START A"},
    },
    {
        .input = {.name = "next key", .text = "AB"},
        .expectation = {.expected_actions = "A RIGHT A START A"},
    },
    {
        .input = {.name = "next row", .text = "AG"},
        .expectation = {.expected_actions = "A DOWN A START A"},
    },
    {
        .input = {.name = "rows wrap", .text = "T"},
        .expectation = {.expected_actions = "UP A START A"},
    },
    {
        .input = {
            .name = "columns wrap through the button column",
            .text = ".",
        },
        .expectation = {.expected_actions = "LEFT LEFT A START A"},
    },
    {
        .input = {.name = "lower case page", .text = "a"},
        .expectation = {.expected_actions = "SELECT A START A"},
    },
    {
        .input = {.name = "symbols page", .text = "0"},
        .expectation = {.expected_actions = "SELECT SELECT A START A"},
    },
    {
        .input = {
            .name = "page change moves the cursor into the symbols columns",
            .text = ".4",
        },
        .expectation = {.expected_actions = "LEFT LEFT A SELECT SELECT LEFT A START A"},
    },
    {
        .input = {.name = "both ways round", .text = "Z"},
        .expectation = {.expected_presses = 7},
    },
    {
        .input = {.name = "male sign", .text = "\xE2\x99\x82"},
        .expectation = {.expected_presses = 9},
    },
    {
        .input = {.name = "full player name", .text = "ABCDEFG"},
        .expectation = {.expected_presses = 19},
    },
    {
        .input = {.name = "mixed case", .text = "Gary"},
        .expectation = {.expected_presses = 16},
    },
    {
        .input = {.name = "space", .text = "A B"},
        .expectation = {.expected_presses = 12},
    },
    {
        .input = {.name = "full nickname", .text = "BULBASAURS", .kind = NamingScreenKind::POKEMON},
        .expectation = {},
    },
    {
        .input = {.name = "female sign", .text = "NIDORAN\xE2\x99\x80", .kind = NamingScreenKind::POKEMON},
        .expectation = {},
    },
    {
        .input = {.name = "box name", .text = "BOX 1-2!", .kind = NamingScreenKind::BOX},
        .expectation = {},
    },
    {
        .input = {.name = "rival", .text = "Blue?", .kind = NamingScreenKind::RIVAL},
        .expectation = {},
    },
    {
        .input = {
            .name = "symbols row 4",
            .text = "\xE2\x80\xA6\xE2\x80\x9C\xE2\x80\x9D\xE2\x80\x98'",
            .kind = NamingScreenKind::POKEMON,
        },
        .expectation = {},
    },
    {
        .input = {.name = "curly apostrophe", .text = "IT\xE2\x80\x99S"},
        .expectation = {.typed = "IT'S"},
    },
};

void add_tests_NamingPlan(UnitTestDatabase& database){
    for (const NamingPlanScenario& scenario : NAMING_PLAN_CASES){
        database.add<Test_NamingPlan>(scenario);
    }
}


struct NamingKeysInput{
    std::string name = "";
    std::string text = "";
    NamingScreenKind kind = NamingScreenKind::PLAYER;
};

struct NamingKeysExpectation{
    //  The exact plan, when the shortest one is unique; "" to skip.
    std::string expected_actions = "";
};

struct NamingKeysScenario{
    NamingKeysInput input;
    NamingKeysExpectation expectation;
};

//  plan_naming_keys() stops before OK: it is plan_naming_input() without the final START A.
class Test_NamingKeys : public UnitTest{
public:
    explicit Test_NamingKeys(NamingKeysScenario scenario)
        : UnitTest("PokemonFRLG::NamingScreen keys only - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string keys = to_string(plan_naming_keys(m_scenario.input.text, m_scenario.input.kind));
        if (!m_scenario.expectation.expected_actions.empty() && keys != m_scenario.expectation.expected_actions){
            return "Expected keys: " + m_scenario.expectation.expected_actions + "\nActual keys:   " + keys;
        }
        const std::string input = to_string(plan_naming_input(m_scenario.input.text, m_scenario.input.kind));
        if (input != keys + " START A"){
            return "Expected the confirmed plan to be \"" + keys + " START A\", got \"" + input + "\"";
        }
        return true;
    }

private:
    const NamingKeysScenario m_scenario;
};

const NamingKeysScenario NAMING_KEYS_CASES[] = {
    {
        .input = {.name = "one key", .text = "A"},
        .expectation = {.expected_actions = "A"},
    },
    {
        .input = {.name = "next key", .text = "AB"},
        .expectation = {.expected_actions = "A RIGHT A"},
    },
    {
        .input = {.name = "lower case page", .text = "a"},
        .expectation = {.expected_actions = "SELECT A"},
    },
    {
        .input = {.name = "full player name", .text = "ABCDEFG"},
        .expectation = {},
    },
    {
        .input = {.name = "mixed case", .text = "Kosmos"},
        .expectation = {},
    },
};

void add_tests_NamingKeys(UnitTestDatabase& database){
    for (const NamingKeysScenario& scenario : NAMING_KEYS_CASES){
        database.add<Test_NamingKeys>(scenario);
    }
}


struct NamingTextInput{
    std::string name = "";
    std::string text = "";
    NamingScreenKind kind = NamingScreenKind::PLAYER;
};

struct NamingTextExpectation{
    //  A part of the expected error; "" if the text is fine.
    std::string expected_error = "";
};

struct NamingTextScenario{
    NamingTextInput input;
    NamingTextExpectation expectation;
};

class Test_NamingText : public UnitTest{
public:
    explicit Test_NamingText(NamingTextScenario scenario)
        : UnitTest("PokemonFRLG::NamingScreen text - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = naming_text_error(m_scenario.input.text, m_scenario.input.kind);
        if (m_scenario.expectation.expected_error.empty()){
            if (!error.empty()){
                return "Expected \"" + m_scenario.input.text + "\" to be accepted, got: " + error;
            }
            return true;
        }
        if (error.find(m_scenario.expectation.expected_error) == std::string::npos){
            return "Expected an error containing \"" + m_scenario.expectation.expected_error + "\", got: \"" + error + "\"";
        }
        try{
            plan_naming_input(m_scenario.input.text, m_scenario.input.kind);
        }catch (InternalProgramError&){
            return true;
        }
        return "Expected plan_naming_input() to throw for \"" + m_scenario.input.text + "\"";
    }

private:
    const NamingTextScenario m_scenario;
};

const NamingTextScenario NAMING_TEXT_CASES[] = {
    {
        .input = {.name = "player name of 7", .text = "ABCDEFG", .kind = NamingScreenKind::PLAYER},
        .expectation = {},
    },
    {
        .input = {.name = "player name of 8", .text = "ABCDEFGH", .kind = NamingScreenKind::PLAYER},
        .expectation = {.expected_error = "at most 7"},
    },
    {
        .input = {.name = "rival name of 7", .text = "ABCDEFG", .kind = NamingScreenKind::RIVAL},
        .expectation = {},
    },
    {
        .input = {.name = "rival name of 8", .text = "ABCDEFGH", .kind = NamingScreenKind::RIVAL},
        .expectation = {.expected_error = "at most 7"},
    },
    {
        .input = {.name = "box name of 8", .text = "ABCDEFGH", .kind = NamingScreenKind::BOX},
        .expectation = {},
    },
    {
        .input = {.name = "box name of 9", .text = "ABCDEFGHI", .kind = NamingScreenKind::BOX},
        .expectation = {.expected_error = "at most 8"},
    },
    {
        .input = {.name = "nickname of 10", .text = "ABCDEFGHIJ", .kind = NamingScreenKind::POKEMON},
        .expectation = {},
    },
    {
        .input = {.name = "nickname of 11", .text = "ABCDEFGHIJK", .kind = NamingScreenKind::POKEMON},
        .expectation = {.expected_error = "at most 10"},
    },
    {
        .input = {
            .name = "symbols count as one character",
            .text = "\xE2\x99\x82\xE2\x99\x82\xE2\x99\x82\xE2\x99\x82\xE2\x99\x82\xE2\x99\x82\xE2\x99\x82",
            .kind = NamingScreenKind::PLAYER,
        },
        .expectation = {},
    },
    {
        .input = {.name = "unknown character", .text = "Ash@", .kind = NamingScreenKind::PLAYER},
        .expectation = {.expected_error = "type: \"@\""},
    },
    {
        .input = {.name = "unknown accented character", .text = "Jos\xC3\xA9", .kind = NamingScreenKind::PLAYER},
        .expectation = {.expected_error = "type: \"\xC3\xA9\""},
    },
    {
        .input = {.name = "empty", .text = "", .kind = NamingScreenKind::PLAYER},
        .expectation = {.expected_error = "empty"},
    },
    {
        .input = {.name = "only spaces", .text = "   ", .kind = NamingScreenKind::PLAYER},
        .expectation = {.expected_error = "only spaces"},
    },
};

void add_tests_NamingText(UnitTestDatabase& database){
    for (const NamingTextScenario& scenario : NAMING_TEXT_CASES){
        database.add<Test_NamingText>(scenario);
    }
}

} // namespace

static void add_tests_AutoStoryReaders(UnitTestDatabase& database){
    add_tests_ItemListReader(database);
    add_tests_OptionMenuReader(database);
    add_tests_SaveCardReader(database);
    add_tests_SummaryMovesReader(database);
}

void add_tests(UnitTestDatabase& database){
    add_tests_AdvanceWhiteDialogDetector(database);
    add_tests_BoxCellOccupiedDetector(database);
    add_tests_BoxCellReader(database);
    add_tests_BoxGenderReader(database);
    add_tests_BoxSelectionModeDetector(database);
    add_tests_SelectionDialogDetector(database);
    add_tests_ShinySymbolDetector(database);
    add_tests_PrizeSelectDetector(database);
    add_tests_AdvanceBattleDialogDetector(database);
    add_tests_BattleMenuDetector(database);
    add_tests_StatsReader(database);
    add_tests_WildEncounterReader(database);
    add_tests_TrainerIdReader(database);
    add_tests_PartyLevelUpReader(database);
    add_tests_BattleLevelUpReader(database);
    add_tests_PartyEmptySlotDetector(database);
    add_tests_PartyHeldItemDetector(database);
    add_tests_PartyMenuDetector(database);
    add_tests_PartySlotDetector(database);
    add_tests_EliteFourRoomDetector(database);
    add_tests_HallOfFameSavingDetector(database);
    add_tests_BattleMoveSelectionArrow(database);
    add_tests_BagDetector(database);
    add_tests_AutoStoryReaders(database);
    add_tests_ExitWildBattle(database);
    add_tests_OverworldMaps(database);
    add_tests_OverworldRoute(database);
    add_tests_OverworldWalk(database);
    add_tests_OverworldReanchor(database);
    add_tests_OverworldRecovery(database);
    add_tests_PlayerFacingReader(database);
    add_tests_AutoStory(database);
    add_tests_AutoStoryStepTable(database);
    add_tests_AutoStoryExport(database);
    add_tests_AutoStorySimulation(database);
    add_tests_AutoStoryItems(database);
    add_tests_AutoStoryRngLoop(database);
    add_tests_NamingCursor(database);
    add_tests_NamingPlan(database);
    add_tests_NamingKeys(database);
    add_tests_NamingText(database);
    add_tests_RngTargetSearch(database);
    add_tests_RngPlanInputs(database);
    add_tests_RngPlan(database);
    add_tests_RngPlanTracker(database);
    add_tests_RngPlanLog(database);
    add_tests_RngSteps(database);
    add_tests_StarterRngLoop(database);
    add_tests_AutoStoryScreenEvents(database);
}


UnitTestResult check_against_golden_file(
    const std::string& image_path,
    const std::vector<std::string>& labels,
    const std::vector<std::string>& values
){
    if (labels.size() != values.size()){
        return "Error: check_against_golden_file() called with " +
               std::to_string(labels.size()) + " labels for " +
               std::to_string(values.size()) + " values.";
    }
    for (size_t c = 0; c < values.size(); c++){
        if (values[c].empty() || values[c].find_first_of(" \t\r\n") != std::string::npos){
            return "Error: value for \"" + labels[c] + "\" is empty or contains whitespace. "
                   "The golden file cannot represent it.";
        }
    }

    Filesystem::Path file_path(image_path);
    Filesystem::Path golden_path =
            file_path.parent_path() / ("_" + file_path.stem().string() + ".txt");

    if (STATIC_GLOBALS.GENERATE_TEST_GOLDEN_FILES){
        std::ofstream output_file(golden_path.stdpath());
        if (!output_file.is_open()){
            return "Error: cannot open " + golden_path.string() + " for writing.";
        }
        for (const std::string& value : values){
            output_file << value << std::endl;
        }
        return true;
    }

    std::vector<std::string> targets;
    if (!load_slug_list(golden_path.string(), targets)){
        return "Error: cannot load golden file " + golden_path.string() + ".";
    }
    if (targets.size() != values.size()){
        return "Error: golden file " + golden_path.string() + " has " +
               std::to_string(targets.size()) + " values, expected " +
               std::to_string(values.size()) + ".";
    }
    for (size_t c = 0; c < values.size(); c++){
        TEST_RESULT_COMPONENT_EQUAL_STR(values[c], targets[c], labels[c]);
    }
    return true;
}


}
}
}