/*  PokemonLZA AutoStory - Dialogs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A table of cases, each a name, the row the cursor is on and the row
 *  wanted, and the presses that get there.
 *
 */

#include <cstddef>
#include <format>
#include <string>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryDialogs.h"
#include "PokemonLZA_AutoStoryDialogs_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


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

} // namespace


void add_tests_AutoStoryDialogs(UnitTestDatabase& database){
    for (const ChoiceMovesCase& item : CHOICE_MOVES_CASES){
        database.add<Test_ChoiceMoves>(item);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
