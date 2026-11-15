/*  PokemonLZA AutoStory - Dialog choice box Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Tests/ImageUnitTest.h"
#include "PokemonLZA_AutoStoryDialogChoiceBox.h"
#include "PokemonLZA_AutoStoryDialogChoiceBox_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr uint32_t PAVEMENT = 0xffc8c3be;
constexpr uint32_t BLUE_WALL = 0xff18375c;
constexpr uint32_t PANEL = 0xff243c6c;
constexpr uint32_t WHITE = 0xffffffff;
constexpr uint32_t DARK_TEXT = 0xff1e1e28;

void fill_box(size_t min_x, size_t min_y, size_t max_x, size_t max_y, uint32_t color, ImageRGB32& image){
    for (size_t y = min_y; y < max_y; y++){
        for (size_t x = min_x; x < max_x; x++){
            image.pixel(x, y) = color;
        }
    }
}

//  A choice menu of `rows` rows drawn as the console shows it (1920 x 1080)
//  on `background`: the panel x 1408 to 1728, from y 808 up, one 80 pitch
//  per row; row k from the bottom highlighted white at y 732 - 80k to
//  800 - 80k, x 1418 to 1715. Each row's text starts at x 1443, in strokes
//  one column in three: dark on the highlighted rows (`highlighted`),
//  white on the others.
ImageRGB32 choice_menu_frame(uint32_t background, size_t rows, std::initializer_list<size_t> highlighted){
    ImageRGB32 image(1920, 1080);
    image.fill(background);
    fill_box(1408, 808 - 80 * rows - 4, 1728, 808, PANEL, image);
    for (size_t row = 0; row < rows; row++){
        const size_t from_bottom = rows - 1 - row;
        const size_t top = 732 - 80 * from_bottom;
        bool lit = false;
        for (const size_t pick : highlighted){
            lit = lit || pick == row;
        }
        if (lit){
            fill_box(1418, top, 1715, top + 68, WHITE, image);
        }
        for (size_t x = 1443; x < 1540; x += 3){
            fill_box(x, top + 16, x + 1, top + 52, lit ? DARK_TEXT : WHITE, image);
        }
    }
    return image;
}

ImageRGB32 flat_frame(uint32_t color){
    ImageRGB32 image(1920, 1080);
    image.fill(color);
    return image;
}

std::string describe(const std::optional<DialogChoiceRows>& rows){
    if (!rows){
        return "no choice menu";
    }
    return std::format("row {} of {}", rows->selected, rows->count);
}

bool same(const std::optional<DialogChoiceRows>& a, const std::optional<DialogChoiceRows>& b){
    if (!a || !b){
        return a.has_value() == b.has_value();
    }
    return a->count == b->count && a->selected == b->selected;
}


struct SyntheticCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        std::optional<DialogChoiceRows> rows;   //  None: no choice menu read.
    };
    Expectation expectation;
};

const SyntheticCase SYNTHETIC_CASES[] = {
    {
        .name = "two rows, the top highlighted",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, 2, {0}); }},
        .expectation = {.rows = DialogChoiceRows{.count = 2, .selected = 0}},
    },
    {
        .name = "two rows, the bottom highlighted",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, 2, {1}); }},
        .expectation = {.rows = DialogChoiceRows{.count = 2, .selected = 1}},
    },
    {
        .name = "three rows on a blue wall, the top highlighted",
        .input = {.frame = []{ return choice_menu_frame(BLUE_WALL, 3, {0}); }},
        .expectation = {.rows = DialogChoiceRows{.count = 3, .selected = 0}},
    },
    {
        .name = "three rows on a blue wall, the middle highlighted",
        .input = {.frame = []{ return choice_menu_frame(BLUE_WALL, 3, {1}); }},
        .expectation = {.rows = DialogChoiceRows{.count = 3, .selected = 1}},
    },
    {
        .name = "four rows, the bottom highlighted",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, 4, {3}); }},
        .expectation = {.rows = DialogChoiceRows{.count = 4, .selected = 3}},
    },
    {
        .name = "three rows, none highlighted",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, 3, {}); }},
        .expectation = {.rows = std::nullopt},
    },
    {
        .name = "three rows, two highlighted",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, 3, {0, 2}); }},
        .expectation = {.rows = std::nullopt},
    },
    {.name = "flat blue wall", .input = {.frame = []{ return flat_frame(BLUE_WALL); }}, .expectation = {.rows = std::nullopt}},
    {.name = "flat panel blue", .input = {.frame = []{ return flat_frame(PANEL); }}, .expectation = {.rows = std::nullopt}},
    {.name = "flat white", .input = {.frame = []{ return flat_frame(WHITE); }}, .expectation = {.rows = std::nullopt}},
    {.name = "flat black", .input = {.frame = []{ return flat_frame(0xff000000); }}, .expectation = {.rows = std::nullopt}},
    {.name = "an empty frame", .input = {.frame = []{ return ImageRGB32(); }}, .expectation = {.rows = std::nullopt}},
    {.name = "a 1 x 1 frame", .input = {.frame = []{ return ImageRGB32(1, 1); }}, .expectation = {.rows = std::nullopt}},
};

class Test_DialogChoiceRowsSynthetic : public UnitTest{
public:
    explicit Test_DialogChoiceRowsSynthetic(const SyntheticCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Dialog choice rows: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<DialogChoiceRows> rows = read_dialog_choice_rows(m_test.input.frame());
        if (!same(rows, m_test.expectation.rows)){
            return std::format("Expected {}, read {}.", describe(m_test.expectation.rows), describe(rows));
        }
        return true;
    }

private:
    const SyntheticCase& m_test;
};


//  the console's choice menus (1920 x 1080), each with the row highlighted as
//  it opens.
struct FrameCase{
    std::string name;
    struct Input{
        std::string_view file;  //  Relative to the test images.
    };
    Input input;
    struct Expectation{
        DialogChoiceRows rows;
    };
    Expectation expectation;
};

const FrameCase FRAME_CASES[] = {
    {
        .name = "the tourist's yes or yes (2 rows, Traditional Chinese)",
        .input = {.file = "PokemonLZA/AutoStoryDetectors/ArrivalToAd/0065-text_box+choice_box.jpg"},
        .expectation = {.rows = {.count = 2, .selected = 0}},
    },
    {
        .name = "the Research Lab elevator's floors (3 rows, Japanese)",
        .input = {.file = "PokemonLZA/AutoStoryDialogChoices/elevator_3rows_row0.png"},
        .expectation = {.rows = {.count = 3, .selected = 0}},
    },
};

class Test_DialogChoiceRowsFrame : public ImageUnitTest{
public:
    explicit Test_DialogChoiceRowsFrame(const FrameCase& test)
        : ImageUnitTest(
            std::format("PokemonLZA::AutoStory - Dialog choice rows: {}", test.name),
            {std::string(test.input.file)}
        )
        , m_test(test)
    {}
    virtual UnitTestResult run_on_images(
        Logger&, CancellableScope&, const std::vector<ImageRGB32>& images
    ) const override{
        const std::optional<DialogChoiceRows> rows = read_dialog_choice_rows(images[0]);
        if (!same(rows, m_test.expectation.rows)){
            return std::format("{}: expected {}, read {}.", image_paths()[0], describe(m_test.expectation.rows), describe(rows));
        }
        return true;
    }

private:
    const FrameCase& m_test;
};

} // namespace


void add_tests_AutoStoryDialogChoiceBox(UnitTestDatabase& database){
    for (const SyntheticCase& test : SYNTHETIC_CASES){
        database.add<Test_DialogChoiceRowsSynthetic>(test);
    }
    for (const FrameCase& test : FRAME_CASES){
        database.add<Test_DialogChoiceRowsFrame>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
