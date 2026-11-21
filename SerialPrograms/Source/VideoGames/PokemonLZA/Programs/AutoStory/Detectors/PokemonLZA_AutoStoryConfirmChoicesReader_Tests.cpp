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

std::string describe_cursor(std::optional<size_t> cursor){
    return cursor ? std::format("row {}", *cursor) : std::string("no Confirm Choices");
}


struct SyntheticCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        std::optional<size_t> cursor;   //  The row read; none: not Confirm Choices.
    };
    Expectation expectation;
};

const SyntheticCase SYNTHETIC_CASES[] = {
    {.name = "row 0 highlighted", .input = {.frame = []{ return confirm_choices_frame({0}); }}, .expectation = {.cursor = 0}},
    {.name = "row 1 highlighted", .input = {.frame = []{ return confirm_choices_frame({1}); }}, .expectation = {.cursor = 1}},
    {
        .name = "Ready! highlighted",
        .input = {.frame = []{ return confirm_choices_frame({CONFIRM_CHOICES_READY_ROW}); }},
        .expectation = {.cursor = CONFIRM_CHOICES_READY_ROW},
    },
    {.name = "no row highlighted", .input = {.frame = []{ return confirm_choices_frame({}); }}, .expectation = {.cursor = std::nullopt}},
    {.name = "two rows highlighted", .input = {.frame = []{ return confirm_choices_frame({0, 1}); }}, .expectation = {.cursor = std::nullopt}},
    {.name = "flat black", .input = {.frame = []{ return flat_frame(0xff000000); }}, .expectation = {.cursor = std::nullopt}},
    {.name = "flat white", .input = {.frame = []{ return flat_frame(WHITE); }}, .expectation = {.cursor = std::nullopt}},
    {.name = "flat background", .input = {.frame = []{ return flat_frame(BACKGROUND); }}, .expectation = {.cursor = std::nullopt}},
    {.name = "flat slate", .input = {.frame = []{ return flat_frame(SLATE); }}, .expectation = {.cursor = std::nullopt}},
    {.name = "an empty frame", .input = {.frame = []{ return ImageRGB32(); }}, .expectation = {.cursor = std::nullopt}},
    {.name = "a 1 x 1 frame", .input = {.frame = []{ return ImageRGB32(1, 1); }}, .expectation = {.cursor = std::nullopt}},
    {.name = "a 40 x 20 frame", .input = {.frame = []{ return ImageRGB32(40, 20); }}, .expectation = {.cursor = std::nullopt}},
};

class Test_ConfirmChoicesSynthetic : public UnitTest{
public:
    explicit Test_ConfirmChoicesSynthetic(const SyntheticCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Confirm Choices reader: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<size_t> cursor = read_confirm_choices_cursor(m_test.input.frame());
        if (cursor != m_test.expectation.cursor){
            return std::format("Expected {}, read {}.", describe_cursor(m_test.expectation.cursor), describe_cursor(cursor));
        }
        return true;
    }

private:
    const SyntheticCase& m_test;
};


//  The folder named by the environment variable `variable`; none if it is
//  unset or not a folder.
std::optional<std::string> env_folder(std::string_view variable){
    const auto* value = std::getenv(std::string(variable).c_str());
    if (value == nullptr){
        return std::nullopt;
    }
    std::string folder(value);
    if (!Filesystem::is_directory(Filesystem::Path(folder))){
        return std::nullopt;
    }
    return folder;
}

//  01.00's start check on console B's intro screens, from the folder in
//  PLZA_INTRO_SCREENS: it accepts Confirm Choices as it opens, and refuses
//  every other intro screen. A file the folder lacks is skipped.
struct CaptureCase{
    std::string name;
    struct Input{
        std::string_view file;
    };
    Input input;
    struct Expectation{
        std::optional<size_t> cursor;
    };
    Expectation expectation;
};

const CaptureCase CAPTURE_CASES[] = {
    {.name = "Confirm Choices as it opens", .input = {.file = "confirm-choices.png"}, .expectation = {.cursor = 0}},
    {.name = "HOME", .input = {.file = "home.png"}, .expectation = {.cursor = std::nullopt}},
    {.name = "a game in front", .input = {.file = "in-game.png"}, .expectation = {.cursor = std::nullopt}},
    {.name = "the language list", .input = {.file = "language-list.png"}, .expectation = {.cursor = std::nullopt}},
    {.name = "the look picker", .input = {.file = "look-picker.png"}, .expectation = {.cursor = std::nullopt}},
    {.name = "the name keyboard", .input = {.file = "keyboard.png"}, .expectation = {.cursor = std::nullopt}},
};

class Test_ConfirmChoicesCapture : public UnitTest{
public:
    explicit Test_ConfirmChoicesCapture(const CaptureCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Confirm Choices reader: {} (PLZA_INTRO_SCREENS)", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<std::string> folder = env_folder("PLZA_INTRO_SCREENS");
        if (!folder){
            return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_INTRO_SCREENS is not a folder.");
        }
        const std::string path = std::format("{}/{}", *folder, m_test.input.file);
        if (!Filesystem::exists(Filesystem::Path(path))){
            return UnitTestResult(UnitTestResult::SKIPPED, std::format("{} is not in the folder.", m_test.input.file));
        }
        const std::optional<size_t> cursor = read_confirm_choices_cursor(ImageRGB32(path));
        if (cursor != m_test.expectation.cursor){
            return std::format(
                "{}: expected {}, read {}.", m_test.input.file, describe_cursor(m_test.expectation.cursor), describe_cursor(cursor)
            );
        }
        return true;
    }

private:
    const CaptureCase& m_test;
};


//  "Confirm Choices" on console B in each game language, one frame per
//  highlighted row (2026-11-13): PokemonLZA/AutoStoryConfirmChoices/
//  <language>_<row>.png, row 0 to CONFIRM_CHOICES_READY_ROW. Each frame
//  reads as its row.
struct LanguageFramesCase{
    std::string name;
    struct Input{
        std::string_view language;
    };
    Input input;
    struct Expectation{
    };
    Expectation expectation;
};

const LanguageFramesCase LANGUAGE_FRAMES_CASES[] = {
    {.name = "English", .input = {.language = "English"}, .expectation = {}},
    {.name = "Spanish", .input = {.language = "Spanish"}, .expectation = {}},
    {.name = "Latin American Spanish", .input = {.language = "SpanishLatAm"}, .expectation = {}},
    {.name = "French", .input = {.language = "French"}, .expectation = {}},
    {.name = "German", .input = {.language = "German"}, .expectation = {}},
    {.name = "Italian", .input = {.language = "Italian"}, .expectation = {}},
    {.name = "Japanese", .input = {.language = "Japanese"}, .expectation = {}},
    {.name = "Korean", .input = {.language = "Korean"}, .expectation = {}},
    {.name = "Simplified Chinese", .input = {.language = "ChineseSimplified"}, .expectation = {}},
    {.name = "Traditional Chinese", .input = {.language = "ChineseTraditional"}, .expectation = {}},
};

std::vector<std::string> language_frame_paths(std::string_view language){
    std::vector<std::string> paths;
    for (size_t row = 0; row <= CONFIRM_CHOICES_READY_ROW; row++){
        paths.push_back(std::format("PokemonLZA/AutoStoryConfirmChoices/{}_{}.png", language, row));
    }
    return paths;
}

class Test_ConfirmChoicesFrames : public ImageUnitTest{
public:
    explicit Test_ConfirmChoicesFrames(const LanguageFramesCase& test)
        : ImageUnitTest(
            std::format("PokemonLZA::AutoStory - Confirm Choices - {}", test.name),
            language_frame_paths(test.input.language)
        )
    {}
    virtual UnitTestResult run_on_images(
        Logger&, CancellableScope&, const std::vector<ImageRGB32>& images
    ) const override{
        for (size_t row = 0; row < images.size(); row++){
            const std::optional<size_t> cursor = read_confirm_choices_cursor(images[row]);
            if (cursor != row){
                return std::format("{}: expected row {}, read {}.", image_paths()[row], row, describe_cursor(cursor));
            }
        }
        return true;
    }
};

} // namespace


void add_tests_AutoStoryConfirmChoicesReader(UnitTestDatabase& database){
    for (const SyntheticCase& test : SYNTHETIC_CASES){
        database.add<Test_ConfirmChoicesSynthetic>(test);
    }
    for (const CaptureCase& test : CAPTURE_CASES){
        database.add<Test_ConfirmChoicesCapture>(test);
    }
    for (const LanguageFramesCase& test : LANGUAGE_FRAMES_CASES){
        database.add<Test_ConfirmChoicesFrames>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
