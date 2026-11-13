/*  PokemonLZA AutoStory - New game intro Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryIntro.h"
#include "PokemonLZA_AutoStoryIntro_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Keys = std::vector<IntroKey>;

Keys repeated(IntroKey key, size_t count){
    return Keys(count, key);
}

Keys joined(std::initializer_list<Keys> parts){
    Keys all;
    for (const Keys& part : parts){
        all.insert(all.end(), part.begin(), part.end());
    }
    return all;
}

UnitTestResult test_language_presses(Logger&){
    struct Row{
        Language language;
        size_t downs;
    };
    const Row rows[] = {
        {Language::English, 0},
        {Language::Spanish, 1},
        {Language::French, 2},
        {Language::German, 3},
        {Language::Italian, 4},
        {Language::Japanese, 5},
        {Language::Korean, 6},
        {Language::ChineseSimplified, 7},
        {Language::ChineseTraditional, 8},
    };
    for (const Row& row : rows){
        const auto presses = language_list_presses(row.language);
        const Keys expected = joined({repeated(IntroKey::DOWN, row.downs), {IntroKey::A}});
        if (!presses || *presses != expected){
            return std::format("{}: expected {} DOWN then A.", language_data(row.language).code, row.downs);
        }
    }
    if (language_list_presses(Language::None)){
        return "Language::None has no entry in the list.";
    }
    return true;
}

UnitTestResult test_look_presses(Logger&){
    //  Look 1 is the top-left face: the clamp, then A.
    const Keys clamp = joined({{IntroKey::UP}, repeated(IntroKey::LEFT, 3)});
    const auto first = look_picker_presses(1);
    if (!first || *first != joined({clamp, {IntroKey::A}})){
        return "Look 1: expected UP, LEFT x3, A.";
    }
    //  Look 3: two to the right on the top row.
    const auto third = look_picker_presses(3);
    if (!third || *third != joined({clamp, repeated(IntroKey::RIGHT, 2), {IntroKey::A}})){
        return "Look 3: expected the clamp, RIGHT x2, A.";
    }
    //  Look 5: the bottom row's first face.
    const auto fifth = look_picker_presses(5);
    if (!fifth || *fifth != joined({clamp, {IntroKey::DOWN, IntroKey::A}})){
        return "Look 5: expected the clamp, DOWN, A.";
    }
    //  Look 8: the bottom-right face.
    const auto last = look_picker_presses(8);
    if (!last || *last != joined({clamp, repeated(IntroKey::RIGHT, 3), {IntroKey::DOWN, IntroKey::A}})){
        return "Look 8: expected the clamp, RIGHT x3, DOWN, A.";
    }
    if (look_picker_presses(0) || look_picker_presses(9)){
        return "Looks 0 and 9 are outside the grid.";
    }
    return true;
}

UnitTestResult test_name_keys(Logger&){
    //  Kosmos: a capital K by Shift, the rest as plain keys.
    const auto kosmos = name_keys("Kosmos", false);
    if (!kosmos || kosmos->keys != "KOSMOS" || !kosmos->shift_first){
        return "Kosmos: expected keys KOSMOS and Shift first.";
    }
    //  A keyboard that capitalizes by itself needs no Shift for it.
    const auto auto_caps = name_keys("Kosmos", true);
    if (!auto_caps || auto_caps->keys != "KOSMOS" || auto_caps->shift_first){
        return "Kosmos on a capitalizing keyboard: expected no Shift.";
    }
    //  A lower case name there needs Shift to undo it, and without it none.
    const auto lower_auto = name_keys("kosmos", true);
    if (!lower_auto || !lower_auto->shift_first){
        return "kosmos on a capitalizing keyboard: expected Shift first.";
    }
    const auto lower = name_keys("kosmos", false);
    if (!lower || lower->shift_first){
        return "kosmos: expected no Shift.";
    }
    //  A digit first takes no Shift.
    const auto digit = name_keys("7up", true);
    if (!digit || digit->keys != "7UP" || digit->shift_first){
        return "7up: expected keys 7UP and no Shift.";
    }
    return true;
}

UnitTestResult test_name_refused(Logger&){
    for (const std::string_view name : {"", "Ko smos", "Kosmos!", "KOsmos", "Kosmo\xC3\x9F"}){
        if (name_keys(name, false)){
            return std::format("\"{}\" is not a name the typer can make.", name);
        }
    }
    return true;
}

UnitTestResult test_new_game_problem(Logger&){
    NewGameChoices choices;
    if (!new_game_problem(Language::English, choices).empty()){
        return "The defaults are a valid new game.";
    }
    if (new_game_problem(Language::None, choices).empty()){
        return "No language is a problem.";
    }
    choices.look = 9;
    if (new_game_problem(Language::English, choices).empty()){
        return "Look 9 is a problem.";
    }
    choices.look = 1;
    choices.name = "KOsmos";
    if (new_game_problem(Language::English, choices).empty()){
        return "A second capital is a problem.";
    }
    return true;
}

struct IntroInput{
    std::string name;
    UnitTestResult (*body)(Logger&);
};

struct IntroExpectation{
};

struct IntroCase{
    IntroInput input;
    IntroExpectation expectation;
};

const IntroCase INTRO_CASES[] = {
    {
        .input = {"Intro: the language list's presses", test_language_presses},
        .expectation = {},
    },
    {
        .input = {"Intro: the look picker's presses", test_look_presses},
        .expectation = {},
    },
    {
        .input = {"Intro: the name's keys and Shift", test_name_keys},
        .expectation = {},
    },
    {
        .input = {"Intro: names the typer refuses", test_name_refused},
        .expectation = {},
    },
    {
        .input = {"Intro: the new game's problems", test_new_game_problem},
        .expectation = {},
    },
};

class Test_AutoStoryIntro : public UnitTest{
public:
    explicit Test_AutoStoryIntro(const IntroCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - {}", test_case.input.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return m_case.input.body(logger);
    }

private:
    const IntroCase& m_case;
};

} // namespace


void add_tests_AutoStoryIntro(UnitTestDatabase& database){
    for (const IntroCase& item : INTRO_CASES){
        database.add<Test_AutoStoryIntro>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
