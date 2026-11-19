/*  PokemonLZA AutoStory - New game Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStoryNewGame.h"
#include "PokemonLZA_AutoStoryNewGameChoices.h"
#include "PokemonLZA_AutoStoryNewGame_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  Presses as letters: U D L R, A.
std::string press_text(const std::optional<std::vector<IntroKey>>& presses){
    if (!presses){
        return "none";
    }
    std::string text;
    for (const IntroKey key : *presses){
        switch (key){
        case IntroKey::UP:
            text += 'U';
            break;
        case IntroKey::DOWN:
            text += 'D';
            break;
        case IntroKey::LEFT:
            text += 'L';
            break;
        case IntroKey::RIGHT:
            text += 'R';
            break;
        case IntroKey::A:
            text += 'A';
            break;
        }
    }
    return text;
}


//  6 and 7 characters of 3 UTF-8 bytes each.
const std::string HANGUL_6 = "\xec\xbd\x94\xec\x8a\xa4\xeb\xaa\xa8\xec\x8a\xa4\xec\xbd\x94\xec\x8a\xa4";
const std::string HANGUL_7 = "\xec\xbd\x94\xec\x8a\xa4\xeb\xaa\xa8\xec\x8a\xa4\xec\xbd\x94\xec\x8a\xa4\xeb\xaa\xa8";


struct ProblemCase{
    std::string name;
    struct Input{
        NewGameChoices choices;
    };
    Input input;
    struct Expectation{
        bool refused = false;
    };
    Expectation expectation;
};

const ProblemCase PROBLEM_CASES[] = {
    {.name = "the defaults", .input = {.choices = {}}, .expectation = {.refused = false}},
    {
        .name = "12 characters in English",
        .input = {.choices = {.name = "Abcdefghijkl"}},
        .expectation = {.refused = false},
    },
    {
        .name = "13 characters in English",
        .input = {.choices = {.name = "Abcdefghijklm"}},
        .expectation = {.refused = true},
    },
    {
        .name = "12 characters in German",
        .input = {.choices = {.language = NewGameLanguage::German, .name = "Abcdefghijkl"}},
        .expectation = {.refused = false},
    },
    {
        .name = "6 characters in Japanese",
        .input = {.choices = {.language = NewGameLanguage::Japanese, .name = "Kosmos"}},
        .expectation = {.refused = false},
    },
    {
        .name = "7 characters in Japanese",
        .input = {.choices = {.language = NewGameLanguage::Japanese, .name = "Kosmoss"}},
        .expectation = {.refused = true},
    },
    {
        .name = "7 characters in Traditional Chinese",
        .input = {.choices = {.language = NewGameLanguage::ChineseTraditional, .name = "Kosmoss"}},
        .expectation = {.refused = true},
    },
    {
        .name = "6 Hangul characters in Korean",
        .input = {.choices = {.language = NewGameLanguage::Korean, .name = HANGUL_6}},
        .expectation = {.refused = false},
    },
    {
        .name = "7 Hangul characters in Korean",
        .input = {.choices = {.language = NewGameLanguage::Korean, .name = HANGUL_7}},
        .expectation = {.refused = true},
    },
    {.name = "an empty name", .input = {.choices = {.name = ""}}, .expectation = {.refused = true}},
    {.name = "look 8", .input = {.choices = {.look = 8}}, .expectation = {.refused = false}},
    {.name = "look 0", .input = {.choices = {.look = 0}}, .expectation = {.refused = true}},
    {.name = "look 9", .input = {.choices = {.look = 9}}, .expectation = {.refused = true}},
};

class Test_NewGameProblem : public UnitTest{
public:
    explicit Test_NewGameProblem(const ProblemCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - new game choices: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<std::string> problem = new_game_problem(m_test.input.choices);
        if (problem.has_value() != m_test.expectation.refused){
            return std::format(
                "Expected {}, got {}.",
                m_test.expectation.refused ? "a refusal" : "no problem",
                problem ? *problem : std::string("no problem")
            );
        }
        return true;
    }

private:
    const ProblemCase& m_test;
};


struct RunProblemCase{
    std::string name;
    struct Input{
        std::vector<std::string> rows;
        NewGameChoices choices;
    };
    Input input;
    struct Expectation{
        bool refused = false;
    };
    Expectation expectation;
};

const RunProblemCase RUN_PROBLEM_CASES[] = {
    {.name = "the defaults from 01.00a", .input = {.rows = {"01.00a", "01.00b"}, .choices = {}}, .expectation = {.refused = false}},
    {
        .name = "Korean from 01.00a",
        .input = {.rows = {"01.00a", "01.00b"}, .choices = {.language = NewGameLanguage::Korean, .name = HANGUL_6}},
        .expectation = {.refused = true},
    },
    {
        .name = "Korean through 01.00a only",
        .input = {.rows = {"01.00a"}, .choices = {.language = NewGameLanguage::Korean, .name = HANGUL_6}},
        .expectation = {.refused = true},
    },
    {
        .name = "Korean from 01.00b",
        .input = {.rows = {"01.00b", "01.01"}, .choices = {.language = NewGameLanguage::Korean, .name = HANGUL_6}},
        .expectation = {.refused = false},
    },
    {.name = "Japanese from 01.00a", .input = {.rows = {"01.00a"}, .choices = {.language = NewGameLanguage::Japanese}}, .expectation = {.refused = false}},
    {.name = "a space in the name from 01.00a", .input = {.rows = {"01.00a"}, .choices = {.name = "Kos mos"}}, .expectation = {.refused = true}},
    {
        .name = "digits and the first page's symbols from 01.00a",
        .input = {.rows = {"01.00a"}, .choices = {.name = "a1-/:',.?!"}},
        .expectation = {.refused = false},
    },
    {.name = "an accented letter from 01.00a", .input = {.rows = {"01.00a"}, .choices = {.name = "Jos\xc3\xa9"}}, .expectation = {.refused = true}},
    {.name = "an accented letter from 01.00b", .input = {.rows = {"01.00b"}, .choices = {.name = "Jos\xc3\xa9"}}, .expectation = {.refused = false}},
    {.name = "a symbol of the second page from 01.00a", .input = {.rows = {"01.00a"}, .choices = {.name = "a#"}}, .expectation = {.refused = true}},
};

std::vector<Pokemon::StoryCheckpoint> builtin_rows(const std::vector<std::string>& ids){
    std::vector<Pokemon::StoryCheckpoint> rows;
    for (const std::string& id : ids){
        for (const Pokemon::StoryCheckpoint& row : builtin_story_data().checkpoints){
            if (row.id == id){
                //  The first step's action says whether the row starts a new game.
                Pokemon::StoryCheckpoint copy{.id = row.id, .title = row.title};
                copy.steps.push_back(Pokemon::StoryStep{.name = row.steps.front().name, .action = row.steps.front().action});
                rows.push_back(std::move(copy));
            }
        }
    }
    return rows;
}

class Test_NewGameRunProblem : public UnitTest{
public:
    explicit Test_NewGameRunProblem(const RunProblemCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - new game choices by run: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<Pokemon::StoryCheckpoint> rows = builtin_rows(m_test.input.rows);
        if (rows.size() != m_test.input.rows.size()){
            return std::string("A row of the case is not in the built-in story.");
        }
        const std::optional<std::string> problem = new_game_run_problem(rows, m_test.input.choices);
        if (problem.has_value() != m_test.expectation.refused){
            return std::format(
                "Expected {}, got {}.",
                m_test.expectation.refused ? "a refusal" : "no problem",
                problem ? *problem : std::string("no problem")
            );
        }
        if (problem && !problem->ends_with(NEW_GAME_BY_HAND_HINT)){
            return std::format("Expected the refusal to end with the hint, got \"{}\".", *problem);
        }
        return true;
    }

private:
    const RunProblemCase& m_test;
};


enum class Grid{
    LANGUAGE_LIST,
    LOOK_PICKER,
};

struct GridCase{
    std::string name;
    struct Input{
        Grid grid = Grid::LANGUAGE_LIST;
        size_t from = 0;
        size_t to = 0;
    };
    Input input;
    struct Expectation{
        std::string presses;
    };
    Expectation expectation;
};

const GridCase GRID_CASES[] = {
    {.name = "English to Deutsch", .input = {.from = 0, .to = 4}, .expectation = {.presses = "DDDD"}},
    {.name = "English to Italiano", .input = {.from = 0, .to = 5}, .expectation = {.presses = "R"}},
    {.name = "English to Traditional Chinese", .input = {.from = 0, .to = 9}, .expectation = {.presses = "RDDDD"}},
    {.name = "Simplified Chinese to Francais", .input = {.from = 8, .to = 3}, .expectation = {.presses = "L"}},
    {.name = "an entry past the list", .input = {.from = 0, .to = 10}, .expectation = {.presses = "none"}},
    {.name = "look 1 to look 8", .input = {.grid = Grid::LOOK_PICKER, .from = 1, .to = 8}, .expectation = {.presses = "RRRD"}},
    {.name = "look 7 to look 2", .input = {.grid = Grid::LOOK_PICKER, .from = 7, .to = 2}, .expectation = {.presses = "LU"}},
    {.name = "look 0", .input = {.grid = Grid::LOOK_PICKER, .from = 0, .to = 2}, .expectation = {.presses = "none"}},
};

class Test_GridMoves : public UnitTest{
public:
    explicit Test_GridMoves(const GridCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - intro moves: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string presses = press_text(m_test.input.grid == Grid::LANGUAGE_LIST
            ? language_list_moves(m_test.input.from, m_test.input.to)
            : look_picker_moves(m_test.input.from, m_test.input.to)
        );
        if (presses != m_test.expectation.presses){
            return std::format("Expected {}, got {}.", m_test.expectation.presses, presses);
        }
        return true;
    }

private:
    const GridCase& m_test;
};

} // namespace


void add_tests_AutoStoryNewGame(UnitTestDatabase& database){
    for (const ProblemCase& test : PROBLEM_CASES){
        database.add<Test_NewGameProblem>(test);
    }
    for (const RunProblemCase& test : RUN_PROBLEM_CASES){
        database.add<Test_NewGameRunProblem>(test);
    }
    for (const GridCase& test : GRID_CASES){
        database.add<Test_GridMoves>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
