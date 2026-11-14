/*  PokemonLZA AutoStory - Confirm Choices reader Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdlib>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Tests/ImageUnitTest.h"
#include "PokemonLZA_AutoStoryConfirmChoicesReader.h"
#include "PokemonLZA_AutoStoryConfirmChoicesReader_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr uint32_t BACKGROUND = 0xff0a1d40;
constexpr uint32_t SLATE = 0xff23344f;
constexpr uint32_t WHITE = 0xffffffff;

//  "Confirm Choices" drawn as console B shows it (1920 x 1080): dark blue,
//  three slate rows x 144 to 742, the two choices from y 578 and 662 (68
//  high) and "Ready!" from y 790 (112 high), the rows in `highlighted`
//  white.
ImageRGB32 confirm_choices_frame(std::initializer_list<size_t> highlighted){
    ImageRGB32 image(1920, 1080);
    image.fill(BACKGROUND);
    const size_t tops[] = {578, 662, 790};
    const size_t heights[] = {68, 68, 112};
    for (size_t row = 0; row <= CONFIRM_CHOICES_READY_ROW; row++){
        bool lit = false;
        for (const size_t pick : highlighted){
            lit = lit || pick == row;
        }
        for (size_t y = tops[row]; y < tops[row] + heights[row]; y++){
            for (size_t x = 144; x < 742; x++){
                image.pixel(x, y) = lit ? WHITE : SLATE;
            }
        }
    }
    return image;
}

ImageRGB32 flat_frame(uint32_t color){
    ImageRGB32 image(1920, 1080);
    image.fill(color);
    return image;
}

struct SyntheticInput{
    std::function<ImageRGB32()> frame;
};

struct SyntheticExpectation{
    //  The row read; nullopt: not Confirm Choices.
    std::optional<size_t> cursor;
};

struct SyntheticCase{
    std::string name;
    SyntheticInput input;
    SyntheticExpectation expectation;
};

const SyntheticCase SYNTHETIC_CASES[] = {
    {"row 0 highlighted", {[]{ return confirm_choices_frame({0}); }}, {0}},
    {"row 1 highlighted", {[]{ return confirm_choices_frame({1}); }}, {1}},
    {"Ready! highlighted", {[]{ return confirm_choices_frame({CONFIRM_CHOICES_READY_ROW}); }}, {CONFIRM_CHOICES_READY_ROW}},
    {"no row highlighted", {[]{ return confirm_choices_frame({}); }}, {std::nullopt}},
    {"two rows highlighted", {[]{ return confirm_choices_frame({0, 1}); }}, {std::nullopt}},
    {"flat black", {[]{ return flat_frame(0xff000000); }}, {std::nullopt}},
    {"flat white", {[]{ return flat_frame(WHITE); }}, {std::nullopt}},
    {"flat background", {[]{ return flat_frame(BACKGROUND); }}, {std::nullopt}},
    {"flat slate", {[]{ return flat_frame(SLATE); }}, {std::nullopt}},
    {"empty frame", {[]{ return ImageRGB32(); }}, {std::nullopt}},
    {"1 x 1 frame", {[]{ return ImageRGB32(1, 1); }}, {std::nullopt}},
    {"40 x 20 frame", {[]{ return ImageRGB32(40, 20); }}, {std::nullopt}},
};

std::string describe_cursor(std::optional<size_t> cursor){
    return cursor ? std::format("row {}", *cursor) : std::string("no Confirm Choices");
}

class Test_ConfirmChoicesSynthetic : public UnitTest{
public:
    explicit Test_ConfirmChoicesSynthetic(const SyntheticCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Confirm Choices reader: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<size_t> cursor = read_confirm_choices_cursor(m_case.input.frame());
        if (cursor != m_case.expectation.cursor){
            return std::format(
                "Expected {}, read {}.", describe_cursor(m_case.expectation.cursor), describe_cursor(cursor)
            );
        }
        return true;
    }

private:
    const SyntheticCase& m_case;
};

//  01.00's start check on console B's frames, from the folder in
//  PLZA_INTRO_SCREENS: it accepts confirm-choices.png, "Change hair
//  color" highlighted as the screen opens, and refuses every other intro
//  screen there.
class Test_ConfirmChoicesCapture : public UnitTest{
public:
    Test_ConfirmChoicesCapture()
        : UnitTest("PokemonLZA::AutoStory - Confirm Choices reader: console B's intro screens (PLZA_INTRO_SCREENS)")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const char* folder = std::getenv("PLZA_INTRO_SCREENS");
        if (folder == nullptr || !Filesystem::is_directory(Filesystem::Path(folder))){
            return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_INTRO_SCREENS is not a folder.");
        }
        const std::optional<size_t> cursor = read_confirm_choices_cursor(ImageRGB32(std::format("{}/confirm-choices.png", folder)));
        if (cursor != size_t(0)){
            return "confirm-choices.png: expected Confirm Choices with row 0 highlighted.";
        }
        for (const std::string_view name : {"home.png", "in-game.png", "language-list.png", "look-picker.png", "keyboard.png"}){
            const std::string path = std::format("{}/{}", folder, name);
            if (!Filesystem::exists(Filesystem::Path(path))){
                continue;
            }
            if (is_confirm_choices(ImageRGB32(path))){
                return std::format("{}: expected 01.00 to refuse it.", name);
            }
        }
        return true;
    }
};

//  "Confirm Choices" on console B in each game language, one frame per
//  highlighted row (2026-11-13). The file name ends in that row:
//  PokemonLZA/AutoStoryConfirmChoices/<language>_<row>.png.
class Test_ConfirmChoicesFrame : public ImageUnitTest{
public:
    explicit Test_ConfirmChoicesFrame(const std::string& image)
        : ImageUnitTest("PokemonLZA::AutoStory - Confirm Choices - " + image, {image})
    {}
    virtual UnitTestResult run_on_images(
        Logger&, CancellableScope&, const std::vector<ImageRGB32>& images
    ) const override{
        const std::string& path = image_paths()[0];
        const size_t underscore = path.rfind('_');
        if (underscore == std::string::npos || underscore + 1 >= path.size()){
            return "Error: the file name does not end in _<row>.";
        }
        const size_t expected = (size_t)(path[underscore + 1] - '0');
        const std::optional<size_t> cursor = read_confirm_choices_cursor(images[0]);
        if (!cursor){
            return std::format("Expected Confirm Choices with row {} highlighted, read no Confirm Choices.", expected);
        }
        if (*cursor != expected){
            return std::format("Expected row {} highlighted, read row {}.", expected, *cursor);
        }
        return true;
    }
};

} // namespace


void add_tests_AutoStoryConfirmChoicesReader(UnitTestDatabase& database){
    for (const SyntheticCase& item : SYNTHETIC_CASES){
        database.add<Test_ConfirmChoicesSynthetic>(item);
    }
    database.add<Test_ConfirmChoicesCapture>();
    for (const std::string_view language : {
        "English", "Spanish", "SpanishLatAm", "French", "German",
        "Italian", "Japanese", "Korean", "ChineseSimplified", "ChineseTraditional",
    }){
        for (size_t row = 0; row <= CONFIRM_CHOICES_READY_ROW; row++){
            database.add<Test_ConfirmChoicesFrame>(std::format("PokemonLZA/AutoStoryConfirmChoices/{}_{}.png", language, row));
        }
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
