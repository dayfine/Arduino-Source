/*  Naming Screen Keyboard Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_NamingScreenKeyboard.h"
#include "PokemonFRLG_NamingScreenKeyboard_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{


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


void add_tests_NamingScreen(UnitTestDatabase& database){
    add_tests_NamingCursor(database);
    add_tests_NamingPlan(database);
    add_tests_NamingKeys(database);
    add_tests_NamingText(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
