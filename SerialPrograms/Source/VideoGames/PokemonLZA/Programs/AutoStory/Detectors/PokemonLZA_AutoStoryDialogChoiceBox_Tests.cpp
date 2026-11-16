/*  PokemonLZA AutoStory - Dialog choice box Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The choice rows reader on synthetic menus drawn as the console shows
 *  them, and on console frames (1920x1080) from CommandLineTests/PokemonLZA/.
 *
 *  Not here: PokemonLZA/AutoStoryScreenEvents/za2_m3w_s2.png, the old
 *  chain's choice box frame for the screen events, is not in
 *  CommandLineTests yet; ArrivalToAd's 0065 stands in for it.
 *
 */

#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenEvents.h"
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
    fill_box(/*min_x=*/1408, 808 - 80 * rows - 4, /*max_x=*/1728, /*max_y=*/808, PANEL, image);
    for (size_t row = 0; row < rows; row++){
        const size_t from_bottom = rows - 1 - row;
        const size_t top = 732 - 80 * from_bottom;
        bool lit = false;
        for (const size_t pick : highlighted){
            lit = lit || pick == row;
        }
        if (lit){
            fill_box(/*min_x=*/1418, top, /*max_x=*/1715, top + 68, WHITE, image);
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
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, /*rows=*/2, /*highlighted=*/{0}); }},
        .expectation = {.rows = DialogChoiceRows{.count = 2, .selected = 0}},
    },
    {
        .name = "two rows, the bottom highlighted",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, /*rows=*/2, /*highlighted=*/{1}); }},
        .expectation = {.rows = DialogChoiceRows{.count = 2, .selected = 1}},
    },
    {
        .name = "three rows on a blue wall, the middle highlighted",
        .input = {.frame = []{ return choice_menu_frame(BLUE_WALL, /*rows=*/3, /*highlighted=*/{1}); }},
        .expectation = {.rows = DialogChoiceRows{.count = 3, .selected = 1}},
    },
    {
        .name = "four rows, the bottom highlighted",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, /*rows=*/4, /*highlighted=*/{3}); }},
        .expectation = {.rows = DialogChoiceRows{.count = 4, .selected = 3}},
    },
    {
        .name = "one highlighted row is no menu",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, /*rows=*/1, /*highlighted=*/{0}); }},
        .expectation = {.rows = std::nullopt},
    },
    {
        .name = "three rows, none highlighted",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, /*rows=*/3, /*highlighted=*/{}); }},
        .expectation = {.rows = std::nullopt},
    },
    {
        .name = "three rows, two highlighted",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, /*rows=*/3, /*highlighted=*/{0, 2}); }},
        .expectation = {.rows = std::nullopt},
    },
    {.name = "flat panel blue", .input = {.frame = []{ return flat_frame(PANEL); }}, .expectation = {.rows = std::nullopt}},
    {.name = "flat white", .input = {.frame = []{ return flat_frame(WHITE); }}, .expectation = {.rows = std::nullopt}},
    {.name = "an empty frame", .input = {.frame = []{ return ImageRGB32(); }}, .expectation = {.rows = std::nullopt}},
    {.name = "a 1 x 1 frame", .input = {.frame = []{ return ImageRGB32(1, 1); }}, .expectation = {.rows = std::nullopt}},
};

UnitTestResult check_synthetic(const SyntheticCase& test){
    const std::optional<DialogChoiceRows> rows = read_dialog_choice_rows(test.input.frame());
    if (!same(rows, test.expectation.rows)){
        return std::format("Expected {}, read {}.", describe(test.expectation.rows), describe(rows));
    }
    return true;
}


//  The console's frames, with the menus' rows as they open (the top one
//  highlighted).
struct FrameCase{
    std::string name;
    struct Input{
        std::string_view file;  //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        std::optional<DialogChoiceRows> rows;   //  None: no choice menu read.
        //  The screen events' CHOICE_BOX (the selection arrow); none: not
        //  checked.
        std::optional<bool> choice_box = std::nullopt;
    };
    Expectation expectation;
};

const FrameCase FRAME_CASES[] = {
    {
        .name = "the tourist's yes or yes (2 rows, Traditional Chinese)",
        .input = {.file = "AutoStoryDetectors/ArrivalToAd/0065-text_box+choice_box.jpg"},
        .expectation = {.rows = DialogChoiceRows{.count = 2, .selected = 0}, .choice_box = true},
    },
    {
        .name = "the bag's \"not really\" (2 rows, Traditional Chinese)",
        .input = {.file = "AutoStoryDetectors/ArrivalToMissionBanner/0173-text_box.jpg"},
        .expectation = {.rows = DialogChoiceRows{.count = 2, .selected = 0}},
    },
    {
        .name = "the Research Lab elevator's floors (3 rows, Japanese)",
        .input = {.file = "AutoStoryDialogChoices/elevator_3rows_row0.png"},
        .expectation = {.rows = DialogChoiceRows{.count = 3, .selected = 0}},
    },
    {
        .name = "light pavement, no menu",
        .input = {.file = "AutoStoryDetectors/ArrivalToAd/0038-nothing+autosave.jpg"},
        .expectation = {.rows = std::nullopt},
    },
};

UnitTestResult check_frame(const FrameCase& test){
    const Filesystem::Path path(std::format("{}PokemonLZA/{}", UNIT_TEST_RESOURCE_PATH(), test.input.file));
    if (!Filesystem::exists(path)){
        return std::format("{} is missing.", path.string());
    }
    const ImageRGB32 frame(path.string());
    const std::optional<DialogChoiceRows> rows = read_dialog_choice_rows(frame);
    if (!same(rows, test.expectation.rows)){
        return std::format("{}: expected {}, read {}.", test.input.file, describe(test.expectation.rows), describe(rows));
    }
    if (!test.expectation.choice_box){
        return true;
    }
    PlzaScreenObserver observer;
    const bool choice_box = observer.read(frame).has(ScreenEvent::CHOICE_BOX);
    if (choice_box != *test.expectation.choice_box){
        return std::format("{}: expected the choice box {}.", test.input.file, *test.expectation.choice_box ? "on" : "off");
    }
    return true;
}


template <typename Case>
class ChoiceBoxTest : public UnitTest{
public:
    using Check = UnitTestResult (*)(const Case&);

    ChoiceBoxTest(std::string_view suite, const Case& test, Check check)
        : UnitTest(std::format("PokemonLZA::AutoStory - Dialog choice rows, {}: {}", suite, test.name))
        , m_test(test)
        , m_check(check)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_check(m_test);
    }

private:
    const Case& m_test;
    Check m_check;
};

} // namespace


void add_tests_AutoStoryDialogChoiceBox(UnitTestDatabase& database){
    for (const SyntheticCase& test : SYNTHETIC_CASES){
        database.add<ChoiceBoxTest<SyntheticCase>>("synthetic", test, check_synthetic);
    }
    for (const FrameCase& test : FRAME_CASES){
        database.add<ChoiceBoxTest<FrameCase>>("console frame", test, check_frame);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
