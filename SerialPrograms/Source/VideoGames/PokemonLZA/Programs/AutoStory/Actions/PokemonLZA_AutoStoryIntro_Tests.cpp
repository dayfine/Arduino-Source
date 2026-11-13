/*  PokemonLZA AutoStory - New game intro Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_AutoStoryIntro.h"
#include "PokemonLZA_AutoStoryIntroScreens.h"
#include "PokemonLZA_AutoStoryLanguageList.h"
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

UnitTestResult test_language_index(Logger&){
    struct Row{
        Language language;
        size_t index;
    };
    //  Down the left column, then the right; entry 2 is Latin American
    //  Spanish, which Language lacks.
    const Row rows[] = {
        {Language::English, 0},
        {Language::Spanish, 1},
        {Language::French, 3},
        {Language::German, 4},
        {Language::Italian, 5},
        {Language::Japanese, 6},
        {Language::Korean, 7},
        {Language::ChineseSimplified, 8},
        {Language::ChineseTraditional, 9},
    };
    for (const Row& row : rows){
        if (language_list_index(row.language) != row.index){
            return std::format("{}: expected entry {}.", language_data(row.language).code, row.index);
        }
    }
    if (language_list_index(Language::None)){
        return "Language::None has no entry in the list.";
    }
    return true;
}

UnitTestResult test_language_moves(Logger&){
    struct Row{
        size_t from;
        size_t to;
        Keys expected;
    };
    const Row rows[] = {
        {0, 0, {}},
        //  English to Deutsch: down the left column.
        {0, 4, repeated(IntroKey::DOWN, 4)},
        //  English to Italiano: across.
        {0, 5, {IntroKey::RIGHT}},
        //  English to Traditional Chinese: across, then down.
        {0, 9, joined({{IntroKey::RIGHT}, repeated(IntroKey::DOWN, 4)})},
        //  Korean to Espanol (Espana): across, then up.
        {7, 1, joined({{IntroKey::LEFT}, repeated(IntroKey::UP, 1)})},
        //  Deutsch to Italiano: no wrap round the bottom edge.
        {4, 5, joined({{IntroKey::RIGHT}, repeated(IntroKey::UP, 4)})},
    };
    for (const Row& row : rows){
        const auto moves = language_list_moves(row.from, row.to);
        if (!moves || *moves != row.expected){
            return std::format("Entry {} to {}: expected {} presses.", row.from, row.to, row.expected.size());
        }
    }
    if (language_list_moves(0, LANGUAGE_LIST_SIZE) || language_list_moves(LANGUAGE_LIST_SIZE, 0)){
        return "Entries outside the list have no moves.";
    }
    return true;
}

//  The language list drawn as console B shows it (1920 x 1080): dark blue,
//  10 slate boxes 576 x 84, the columns at x 347 and 996, the rows from
//  y 342, 108 apart, entry `highlighted` (if any) white.
ImageRGB32 language_list_frame(std::optional<size_t> highlighted){
    ImageRGB32 image(1920, 1080);
    image.fill(0xff091838);
    for (size_t index = 0; index < LANGUAGE_LIST_SIZE; index++){
        const size_t left = index < LANGUAGE_LIST_ROWS ? 347 : 996;
        const size_t top = 342 + 108 * (index % LANGUAGE_LIST_ROWS);
        const uint32_t color = highlighted == index ? 0xffffffff : 0xff363c53;
        for (size_t y = top; y < top + 84; y++){
            for (size_t x = left; x < left + 576; x++){
                image.pixel(x, y) = color;
            }
        }
    }
    return image;
}

UnitTestResult test_language_list_synthetic(Logger&){
    for (size_t index = 0; index < LANGUAGE_LIST_SIZE; index++){
        if (read_language_list_cursor(language_list_frame(index)) != index){
            return std::format("Expected the highlight read on entry {}.", index);
        }
    }
    if (is_language_list(language_list_frame(std::nullopt))){
        return "Expected no list without a highlight.";
    }
    ImageRGB32 two = language_list_frame(0);
    for (size_t y = 450; y < 534; y++){
        for (size_t x = 347; x < 923; x++){
            two.pixel(x, y) = 0xffffffff;
        }
    }
    if (is_language_list(two)){
        return "Expected no list with two highlights.";
    }
    for (const uint32_t fill : {0xff000000u, 0xffffffffu, 0xff363c53u}){
        ImageRGB32 flat(1920, 1080);
        flat.fill(fill);
        if (is_language_list(flat)){
            return std::format("Expected a flat {:08x} frame not to be the list.", fill);
        }
    }
    return true;
}

UnitTestResult test_language_list_empty(Logger&){
    if (is_language_list(ImageRGB32()) || is_language_list(ImageRGB32(1, 1)) || is_language_list(ImageRGB32(40, 20))){
        return "Expected empty and tiny frames not to be the list.";
    }
    return true;
}

//  The console's frame, from the folder in PLZA_INTRO_SCREENS
//  (~/BDSPLab/intro-screens/plza on the lab Mac): language-list.png, the
//  list as a new game opens it, English highlighted.
UnitTestResult test_language_list_capture(Logger&){
    const auto* folder = std::getenv("PLZA_INTRO_SCREENS");
    if (folder == nullptr || !Filesystem::is_directory(Filesystem::Path(folder))){
        return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_INTRO_SCREENS is not a folder.");
    }
    const ImageRGB32 frame(std::format("{}/language-list.png", folder));
    const std::optional<size_t> cursor = read_language_list_cursor(frame);
    if (!cursor){
        return "language-list.png: expected the language list, read none.";
    }
    if (*cursor != 0){
        return std::format("language-list.png: expected English (entry 0) highlighted, read entry {}.", *cursor);
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
        .input = {"Intro: the language list's entries", test_language_index},
        .expectation = {},
    },
    {
        .input = {"Intro: the language list's cursor moves", test_language_moves},
        .expectation = {},
    },
    {
        .input = {"Intro: the language list's highlight, synthetic frames", test_language_list_synthetic},
        .expectation = {},
    },
    {
        .input = {"Intro: empty frames are not the language list", test_language_list_empty},
        .expectation = {},
    },
    {
        .input = {"Intro: the language list on console B (PLZA_INTRO_SCREENS)", test_language_list_capture},
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
