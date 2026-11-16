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


//  new_game_problem(): whether the program refuses the choices before any
//  press.
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
        .name = "Korean is not supported",
        .input = {.choices = {.language = NewGameLanguage::Korean, .name = "Kosmos"}},
        .expectation = {.refused = true},
    },
    {.name = "an empty name", .input = {.choices = {.name = ""}}, .expectation = {.refused = true}},
    {.name = "a space in the name", .input = {.choices = {.name = "Kos mos"}}, .expectation = {.refused = true}},
    {.name = "digits and the first page's symbols", .input = {.choices = {.name = "a1-/:',.?!"}}, .expectation = {.refused = false}},
    {.name = "an accented letter", .input = {.choices = {.name = "Jos\xc3\xa9"}}, .expectation = {.refused = true}},
    {.name = "a symbol of the second page", .input = {.choices = {.name = "a#"}}, .expectation = {.refused = true}},
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


//  The language list's moves (entries down the left column, then the right
//  one) and the look picker's (looks along the top row, then the bottom).
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


//  Capture mode's recording: change 4, at most one frame a second. Each
//  frame is (ms since the launch, RMSD from the last saved; -1: none yet).
struct RecorderCase{
    std::string name;
    struct Input{
        std::vector<std::pair<int, double>> frames;
    };
    Input input;
    struct Expectation{
        std::string saved;      //  One letter per frame: s saved, . not.
    };
    Expectation expectation;
};

const RecorderCase RECORDER_CASES[] = {
    {.name = "the first frame is saved", .input = {.frames = {{0, -1}}}, .expectation = {.saved = "s"}},
    {
        .name = "a still screen is saved once",
        .input = {.frames = {{0, -1}, {1500, 1}, {3000, 2}}},
        .expectation = {.saved = "s.."},
    },
    {
        .name = "each new screen is saved",
        .input = {.frames = {{0, -1}, {1500, 30}, {3000, 3}, {4500, 25}}},
        .expectation = {.saved = "ss.s"},
    },
    {
        .name = "a change within a second of the last save waits",
        .input = {.frames = {{0, -1}, {500, 30}, {1250, 30}}},
        .expectation = {.saved = "s.s"},
    },
};

class Test_IntroRecorder : public UnitTest{
public:
    explicit Test_IntroRecorder(const RecorderCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - capture recording: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        IntroFrameRecorder recorder(/*change=*/4, /*every=*/Milliseconds(1000));
        std::string saved;
        for (const auto& [at, change] : m_test.input.frames){
            const std::optional<double> from_saved = change < 0 ? std::nullopt : std::optional<double>(change);
            saved += recorder.see(Milliseconds(at), from_saved) ? 's' : '.';
        }
        if (saved != m_test.expectation.saved){
            return std::format("Expected {}, got {}.", m_test.expectation.saved, saved);
        }
        return true;
    }

private:
    const RecorderCase& m_test;
};


//  Capture mode's file names, which sort each language's screens in order.
struct CaptureFileCase{
    std::string name;
    struct Input{
        NewGameLanguage language = NewGameLanguage::English;
        std::string screen;
    };
    Input input;
    struct Expectation{
        std::string file;
    };
    Expectation expectation;
};

const CaptureFileCase CAPTURE_FILE_CASES[] = {
    {.name = "a screen before the list", .input = {.language = NewGameLanguage::German, .screen = "03"}, .expectation = {.file = "de_03.png"}},
    {.name = "Korean is captured", .input = {.language = NewGameLanguage::Korean, .screen = "keyboard"}, .expectation = {.file = "ko_keyboard.png"}},
    {
        .name = "Latin American Spanish",
        .input = {.language = NewGameLanguage::SpanishLatinAmerica, .screen = "language-list"},
        .expectation = {.file = "es-419_language-list.png"},
    },
};

class Test_CaptureFile : public UnitTest{
public:
    explicit Test_CaptureFile(const CaptureFileCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - capture file: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string file = intro_capture_file(m_test.input.language, m_test.input.screen);
        if (file != m_test.expectation.file){
            return std::format("Expected {}, got {}.", m_test.expectation.file, file);
        }
        return true;
    }

private:
    const CaptureFileCase& m_test;
};

} // namespace


void add_tests_AutoStoryNewGame(UnitTestDatabase& database){
    for (const ProblemCase& test : PROBLEM_CASES){
        database.add<Test_NewGameProblem>(test);
    }
    for (const GridCase& test : GRID_CASES){
        database.add<Test_GridMoves>(test);
    }
    for (const RecorderCase& test : RECORDER_CASES){
        database.add<Test_IntroRecorder>(test);
    }
    for (const CaptureFileCase& test : CAPTURE_FILE_CASES){
        database.add<Test_CaptureFile>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
