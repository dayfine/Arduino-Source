/*  PokemonLZA AutoStory - Dialogs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Tables of cases: each a name, the row the cursor is on and the row
 *  wanted, and the presses that get there; or a name, the looks of A
 *  through dialogs until quiet, and what each look does.
 *
 */

#include <cstddef>
#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryDialogs.h"
#include "PokemonLZA_AutoStoryDialogs_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  dialog_choice_moves(): the D-pad presses from one choice row to another.
struct ChoiceMovesCase{
    std::string name;
    struct Input{
        size_t current;
        size_t target;
    };
    Input input;
    struct Expectation{
        int moves;  //  down when positive, up when negative
    };
    Expectation expectation;
};

const ChoiceMovesCase CHOICE_MOVES_CASES[] = {
    {
        .name = "opens on the top row, the top choice: no press",
        .input = {.current = 0, .target = 0},
        .expectation = {.moves = 0},
    },
    {
        .name = "opens on the top row, the second choice: one down",
        .input = {.current = 0, .target = 1},
        .expectation = {.moves = 1},
    },
    {
        .name = "opens on the top row, the third choice: two down",
        .input = {.current = 0, .target = 2},
        .expectation = {.moves = 2},
    },
    {
        .name = "opens on the second row, the second choice: no press",
        .input = {.current = 1, .target = 1},
        .expectation = {.moves = 0},
    },
    {
        .name = "opens on the second row, the top choice: one up",
        .input = {.current = 1, .target = 0},
        .expectation = {.moves = -1},
    },
};

class Test_ChoiceMoves : public UnitTest{
public:
    explicit Test_ChoiceMoves(const ChoiceMovesCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Dialogs: choice moves: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const int moves = dialog_choice_moves(m_case.input.current, m_case.input.target);
        if (moves != m_case.expectation.moves){
            return std::format("Expected {} moves, got {}.", m_case.expectation.moves, moves);
        }
        return true;
    }

private:
    const ChoiceMovesCase& m_case;
};

//  QuietPresses: what each look does, quiet 3 s and timeout 10 s.
struct QuietPressesCase{
    std::string name;
    struct Look{
        Milliseconds at;    //  after the presses were sent
        bool busy;          //  a dialog or a tutorial card on screen
    };
    struct Input{
        std::vector<Look> looks;
    };
    Input input;
    struct Expectation{
        std::vector<QuietPresses::Step> steps;
    };
    Expectation expectation;
};

const QuietPressesCase QUIET_PRESSES_CASES[] = {
    {
        //  03.10's claim: the menu's A and its 3 s
        //  ran before the first look, which ended the claim on one look.
        .name = "first look 5 s after the presses, nothing busy: the quiet time starts there",
        .input = {.looks = {{.at = 5000ms, .busy = false}, {.at = 7000ms, .busy = false}, {.at = 8000ms, .busy = false}}},
        .expectation = {.steps = {QuietPresses::Step::WAIT, QuietPresses::Step::WAIT, QuietPresses::Step::QUIET}},
    },
    {
        .name = "busy looks get A, the quiet time counts from the last",
        .input = {.looks = {{.at = 0ms, .busy = true}, {.at = 1000ms, .busy = true}, {.at = 3500ms, .busy = false}, {.at = 4000ms, .busy = false}}},
        .expectation = {.steps = {QuietPresses::Step::PRESS_A, QuietPresses::Step::PRESS_A, QuietPresses::Step::WAIT, QuietPresses::Step::QUIET}},
    },
    {
        .name = "still busy at the timeout from the first look: timed out",
        .input = {.looks = {{.at = 2000ms, .busy = true}, {.at = 7000ms, .busy = true}, {.at = 12000ms, .busy = true}}},
        .expectation = {.steps = {QuietPresses::Step::PRESS_A, QuietPresses::Step::PRESS_A, QuietPresses::Step::TIMED_OUT}},
    },
};

class Test_QuietPresses : public UnitTest{
public:
    explicit Test_QuietPresses(const QuietPressesCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Dialogs: quiet presses: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const WallClock sent = current_time();
        QuietPresses clock(3000ms, 10000ms);
        const std::vector<Look>& looks = m_case.input.looks;
        for (size_t i = 0; i < looks.size(); i++){
            const QuietPresses::Step step = clock.look(looks[i].busy, sent + looks[i].at);
            if (step != m_case.expectation.steps[i]){
                return std::format("Look {}: expected step {}, got {}.", i, (int)m_case.expectation.steps[i], (int)step);
            }
        }
        return true;
    }

private:
    using Look = QuietPressesCase::Look;
    const QuietPressesCase& m_case;
};

} // namespace


void add_tests_AutoStoryDialogs(UnitTestDatabase& database){
    for (const ChoiceMovesCase& item : CHOICE_MOVES_CASES){
        database.add<Test_ChoiceMoves>(item);
    }
    for (const QuietPressesCase& item : QUIET_PRESSES_CASES){
        database.add<Test_QuietPresses>(item);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
