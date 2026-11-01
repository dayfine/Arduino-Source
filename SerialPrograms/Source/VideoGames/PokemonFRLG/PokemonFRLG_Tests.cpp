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
#include "Inference/Box/PokemonFRLG_BoxCellOccupiedDetector.h"
#include "Inference/Box/PokemonFRLG_BoxGenderReader.h"
#include "Inference/Box/PokemonFRLG_BoxSelectionModeDetector.h"
#include "Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "Inference/Dialogs/PokemonFRLG_PrizeSelectDetector.h"
#include "Inference/Map/PokemonFRLG_PokemonLeagueDetectors.h"
#include "Inference/Menus/PokemonFRLG_BagDetector.h"
#include "Inference/Menus/PokemonFRLG_PartyEmptySlotDetector.h"
#include "Inference/Menus/PokemonFRLG_PartyHeldItemDetector.h"
#include "Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "Inference/PokemonFRLG_BattleLevelUpReader.h"
#include "Inference/PokemonFRLG_BattleSelectionArrowDetector.h"
#include "Inference/PokemonFRLG_PartyLevelUpReader.h"
#include "Inference/PokemonFRLG_ShinySymbolDetector.h"
#include "Inference/PokemonFRLG_StatsReader.h"
#include "Inference/PokemonFRLG_TrainerIdReader.h"
#include "Inference/PokemonFRLG_WildEncounterReader.h"
#include "Programs/PokemonFRLG_NamingScreenKeyboard.h"
#include "PokemonFRLG_Navigation.h"
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

struct NamingCursorScenario{
    std::string name = "";
    NamingCursor start = {};
    std::vector<NamingAction> actions = {};
    NamingCursor expected = {};
};

class Test_NamingCursor : public UnitTest{
public:
    Test_NamingCursor(NamingCursorScenario scenario)
        : UnitTest("PokemonFRLG::NamingScreen cursor - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        NamingCursor cursor = m_scenario.start;
        for (NamingAction action : m_scenario.actions){
            cursor = apply_naming_action(cursor, action);
        }
        if (cursor != m_scenario.expected){
            return "After " + to_string(m_scenario.actions) + ": expected " + to_string(m_scenario.expected)
                + ", got " + to_string(cursor);
        }
        return true;
    }

private:
    const NamingCursorScenario m_scenario;
};

//  Crossing and moving on the button column.
std::vector<NamingCursorScenario> naming_cursor_button_column_scenarios(){
    using enum NamingAction;
    using enum NamingScreenPage;
    return {
        {
            .name = "left from A goes to the page button",
            .actions = {LEFT},
            .expected = {.page = UPPER, .row = 0, .column = 8, .remembered_row = 1},
        },
        {
            .name = "left and right return to A",
            .actions = {LEFT, RIGHT},
            .expected = {.page = UPPER, .row = 0, .column = 0, .remembered_row = 1},
        },
        {
            //  Rows 1 and 2 share the back button; the game remembers which one it came from.
            .name = "row 1 crosses the button column",
            .start = {.page = UPPER, .row = 1, .column = 0},
            .actions = {LEFT, RIGHT},
            .expected = {.page = UPPER, .row = 1, .column = 0, .remembered_row = 1},
        },
        {
            .name = "row 2 crosses the button column",
            .start = {.page = UPPER, .row = 2, .column = 0},
            .actions = {LEFT, RIGHT},
            .expected = {.page = UPPER, .row = 2, .column = 0, .remembered_row = 2},
        },
        {
            .name = "row 3 crosses the OK button",
            .start = {.page = UPPER, .row = 3, .column = 0},
            .actions = {LEFT, RIGHT},
            .expected = {.page = UPPER, .row = 3, .column = 0, .remembered_row = 2},
        },
        {
            .name = "right from the last column wraps through the buttons",
            .start = {.page = UPPER, .row = 0, .column = 7},
            .actions = {RIGHT, RIGHT},
            .expected = {.page = UPPER, .row = 0, .column = 0, .remembered_row = 1},
        },
        {
            //  Moving on the button column overwrites the remembered row with BACK.
            .name = "moving on the button column forgets row 2",
            .start = {.page = UPPER, .row = 2, .column = 0},
            .actions = {LEFT, UP, DOWN, RIGHT},
            .expected = {.page = UPPER, .row = 1, .column = 0, .remembered_row = 1},
        },
    };
}
//  Wrap-around on the keys and the button column.
std::vector<NamingCursorScenario> naming_cursor_wrap_scenarios(){
    using enum NamingAction;
    using enum NamingScreenPage;
    return {
        {
            .name = "button column wraps over three buttons",
            .start = {.page = UPPER, .row = 0, .column = 8, .remembered_row = 1},
            .actions = {UP},
            .expected = {.page = UPPER, .row = 2, .column = 8, .remembered_row = 2},
        },
        {
            .name = "rows wrap up",
            .actions = {UP},
            .expected = {.page = UPPER, .row = 3, .column = 0},
        },
        {
            .name = "rows wrap down",
            .start = {.page = UPPER, .row = 3, .column = 0},
            .actions = {DOWN},
            .expected = {.page = UPPER, .row = 0, .column = 0},
        },
        {
            .name = "symbols page has six key columns",
            .start = {.page = SYMBOLS, .row = 0, .column = 0},
            .actions = {LEFT, LEFT},
            .expected = {.page = SYMBOLS, .row = 0, .column = 5, .remembered_row = 1},
        },
    };
}
//  SELECT and START.
std::vector<NamingCursorScenario> naming_cursor_page_scenarios(){
    using enum NamingAction;
    using enum NamingScreenPage;
    return {
        {
            .name = "SELECT goes to lower case",
            .actions = {SELECT},
            .expected = {.page = LOWER, .row = 0, .column = 0},
        },
        {
            .name = "SELECT twice goes to symbols",
            .actions = {SELECT, SELECT},
            .expected = {.page = SYMBOLS, .row = 0, .column = 0},
        },
        {
            .name = "SELECT three times returns to upper case",
            .actions = {SELECT, SELECT, SELECT},
            .expected = {.page = UPPER, .row = 0, .column = 0},
        },
        {
            .name = "SELECT moves the cursor into the symbols columns",
            .start = {.page = UPPER, .row = 0, .column = 7},
            .actions = {SELECT, SELECT, SELECT},
            .expected = {.page = UPPER, .row = 0, .column = 5},
        },
        {
            .name = "SELECT keeps the cursor on the button column",
            .start = {.page = UPPER, .row = 1, .column = 8, .remembered_row = 1},
            .actions = {SELECT, SELECT},
            .expected = {.page = SYMBOLS, .row = 1, .column = 6, .remembered_row = 1},
        },
        {
            .name = "START goes to OK",
            .start = {.page = LOWER, .row = 1, .column = 3},
            .actions = {START},
            .expected = {.page = LOWER, .row = 2, .column = 8},
        },
        {
            .name = "START goes to OK on symbols",
            .start = {.page = SYMBOLS, .row = 0, .column = 0},
            .actions = {START},
            .expected = {.page = SYMBOLS, .row = 2, .column = 6},
        },
    };
}
void add_tests_NamingCursor(UnitTestDatabase& database){
    for (auto scenarios : {
        naming_cursor_button_column_scenarios(),
        naming_cursor_wrap_scenarios(),
        naming_cursor_page_scenarios(),
    }){
        for (const NamingCursorScenario& scenario : scenarios){
            database.add<Test_NamingCursor>(scenario);
        }
    }
}


struct NamingPlanScenario{
    std::string name = "";
    std::string text = "";
    NamingScreenKind kind = NamingScreenKind::PLAYER;
    //  What the game shows when it differs from `text` (another spelling of a key).
    std::string typed = "";
    //  The exact plan, when the shortest one is unique; "" to skip.
    std::string expected_actions = "";
    //  The number of presses, including A and the final START A; 0 to skip.
    size_t expected_presses = 0;
};

class Test_NamingPlan : public UnitTest{
public:
    Test_NamingPlan(NamingPlanScenario scenario)
        : UnitTest("PokemonFRLG::NamingScreen plan - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<NamingAction> actions = plan_naming_input(m_scenario.text, m_scenario.kind);
        const std::string plan = to_string(actions);
        if (!m_scenario.expected_actions.empty() && plan != m_scenario.expected_actions){
            return "Expected plan: " + m_scenario.expected_actions + "\nActual plan:   " + plan;
        }
        if (m_scenario.expected_presses != 0 && actions.size() != m_scenario.expected_presses){
            return "Expected " + std::to_string(m_scenario.expected_presses) + " presses, got "
                + std::to_string(actions.size()) + ": " + plan;
        }

        //  Replay the plan: every A but the last types the key under the cursor, and the last
        //  presses OK.
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
        const std::string& expected = m_scenario.typed.empty() ? m_scenario.text : m_scenario.typed;
        if (typed != expected){
            return "Expected the plan to type \"" + expected + "\", it types \"" + typed + "\": " + plan;
        }
        return true;
    }

private:
    const NamingPlanScenario m_scenario;
};

void add_tests_NamingPlan(UnitTestDatabase& database){
    using enum NamingScreenKind;
    const NamingPlanScenario scenarios[] = {
        {.name = "starts on A", .text = "A", .expected_actions = "A START A"},
        {.name = "next key", .text = "AB", .expected_actions = "A RIGHT A START A"},
        {.name = "next row", .text = "AG", .expected_actions = "A DOWN A START A"},
        {.name = "rows wrap", .text = "T", .expected_actions = "UP A START A"},
        {
            .name = "columns wrap through the button column",
            .text = ".",
            .expected_actions = "LEFT LEFT A START A",
        },
        {.name = "lower case page", .text = "a", .expected_actions = "SELECT A START A"},
        {.name = "symbols page", .text = "0", .expected_actions = "SELECT SELECT A START A"},
        {
            //  The cursor moves from column 7 to the symbols page's last column, 5.
            .name = "page change moves the cursor into the symbols columns",
            .text = ".4",
            .expected_actions = "LEFT LEFT A SELECT SELECT LEFT A START A",
        },
        {.name = "both ways round", .text = "Z", .expected_presses = 7},
        {.name = "male sign", .text = "\xE2\x99\x82", .expected_presses = 9},
        {.name = "full player name", .text = "ABCDEFG", .expected_presses = 19},
        {.name = "mixed case", .text = "Gary", .expected_presses = 16},
        {.name = "space", .text = "A B", .expected_presses = 12},
        {.name = "full nickname", .text = "BULBASAURS", .kind = POKEMON},
        {.name = "female sign", .text = "NIDORAN\xE2\x99\x80", .kind = POKEMON},
        {.name = "box name", .text = "BOX 1-2!", .kind = BOX},
        {.name = "rival", .text = "Blue?", .kind = RIVAL},
        {
            .name = "symbols row 4",
            .text = "\xE2\x80\xA6\xE2\x80\x9C\xE2\x80\x9D\xE2\x80\x98'",
            .kind = POKEMON,
        },
        {.name = "curly apostrophe", .text = "IT\xE2\x80\x99S", .typed = "IT'S"},
    };
    for (const NamingPlanScenario& scenario : scenarios){
        database.add<Test_NamingPlan>(scenario);
    }
}


struct NamingTextScenario{
    std::string name = "";
    std::string text = "";
    NamingScreenKind kind = NamingScreenKind::PLAYER;
    //  A part of the expected error; "" if the text is fine.
    std::string expected_error = "";
};

class Test_NamingText : public UnitTest{
public:
    Test_NamingText(NamingTextScenario scenario)
        : UnitTest("PokemonFRLG::NamingScreen text - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = naming_text_error(m_scenario.text, m_scenario.kind);
        if (m_scenario.expected_error.empty()){
            if (!error.empty()){
                return "Expected \"" + m_scenario.text + "\" to be accepted, got: " + error;
            }
            return true;
        }
        if (error.find(m_scenario.expected_error) == std::string::npos){
            return "Expected an error containing \"" + m_scenario.expected_error + "\", got: \"" + error + "\"";
        }
        try{
            plan_naming_input(m_scenario.text, m_scenario.kind);
        }catch (InternalProgramError&){
            return true;
        }
        return "Expected plan_naming_input() to throw for \"" + m_scenario.text + "\"";
    }

private:
    const NamingTextScenario m_scenario;
};

void add_tests_NamingText(UnitTestDatabase& database){
    using enum NamingScreenKind;
    const NamingTextScenario scenarios[] = {
        {.name = "player name of 7", .text = "ABCDEFG", .kind = PLAYER},
        {.name = "player name of 8", .text = "ABCDEFGH", .kind = PLAYER, .expected_error = "at most 7"},
        {.name = "rival name of 7", .text = "ABCDEFG", .kind = RIVAL},
        {.name = "rival name of 8", .text = "ABCDEFGH", .kind = RIVAL, .expected_error = "at most 7"},
        {.name = "box name of 8", .text = "ABCDEFGH", .kind = BOX},
        {.name = "box name of 9", .text = "ABCDEFGHI", .kind = BOX, .expected_error = "at most 8"},
        {.name = "nickname of 10", .text = "ABCDEFGHIJ", .kind = POKEMON},
        {.name = "nickname of 11", .text = "ABCDEFGHIJK", .kind = POKEMON, .expected_error = "at most 10"},
        {
            //  Seven characters, 21 bytes.
            .name = "symbols count as one character",
            .text = "\xE2\x99\x82\xE2\x99\x82\xE2\x99\x82\xE2\x99\x82\xE2\x99\x82\xE2\x99\x82\xE2\x99\x82",
        },
        {.name = "unknown character", .text = "Ash@", .expected_error = "type: \"@\""},
        {.name = "unknown accented character", .text = "Jos\xC3\xA9", .expected_error = "type: \"\xC3\xA9\""},
        {.name = "empty", .text = "", .expected_error = "empty"},
        {.name = "only spaces", .text = "   ", .expected_error = "only spaces"},
    };
    for (const NamingTextScenario& scenario : scenarios){
        database.add<Test_NamingText>(scenario);
    }
}

}


void add_tests(UnitTestDatabase& database){
    add_tests_AdvanceWhiteDialogDetector(database);
    add_tests_BoxCellOccupiedDetector(database);
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
    add_tests_ExitWildBattle(database);
    add_tests_NamingCursor(database);
    add_tests_NamingPlan(database);
    add_tests_NamingText(database);
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