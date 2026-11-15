/*  PokemonLZA AutoStory - Dialog Detector Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <string>
#include <string_view>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_AutoStoryDialogDetector.h"
#include "PokemonLZA_AutoStoryDialogDetector_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr size_t WIDTH = 1920;
constexpr size_t HEIGHT = 1080;
constexpr uint32_t WHITE = 0xffffffff;
constexpr uint32_t DARK_GRAY = 0xff303030;

void fill_box(size_t min_x, size_t min_y, size_t max_x, size_t max_y, uint32_t color, ImageRGB32& image){
    for (size_t y = min_y; y < max_y; y++){
        for (size_t x = min_x; x < max_x; x++){
            image.pixel(x, y) = color;
        }
    }
}

ImageRGB32 solid_frame(uint32_t color){
    ImageRGB32 image(WIDTH, HEIGHT);
    image.fill(color);
    return image;
}

//  A white dialogue box's bottom edge (x 500-1410, y 990-1040) on a dark
//  scene, with no advance arrow: the box while its text still types.
ImageRGB32 box_without_arrow_frame(){
    ImageRGB32 image = solid_frame(DARK_GRAY);
    fill_box(500, 990, 1410, 1040, WHITE, image);
    return image;
}

//  A console frame from CommandLineTests/PokemonLZA/.
ImageRGB32 console_frame(std::string_view path){
    return ImageRGB32(std::format("{}PokemonLZA/{}", UNIT_TEST_RESOURCE_PATH(), path));
}

//  The console frame `path` with the advance arrow's area (x 1395-1470,
//  y 935-1030) painted white, as the box shows before its text is done.
ImageRGB32 console_frame_without_arrow(std::string_view path){
    ImageRGB32 image = console_frame(path);
    fill_box(1395, 935, 1470, 1030, WHITE, image);
    return image;
}


struct DialogCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
        WhiteDialogPart white;
    };
    Input input;
    struct Expectation{
        bool dialog;
    };
    Expectation expectation;
};

const DialogCase DIALOG_CASES[] = {
    {
        .name = "a black frame, box and arrow",
        .input = {.frame = []{ return solid_frame(0xff000000); }, .white = WhiteDialogPart::BOX_AND_ARROW},
        .expectation = {.dialog = false},
    },
    {
        .name = "a black frame, box only",
        .input = {.frame = []{ return solid_frame(0xff000000); }, .white = WhiteDialogPart::BOX_ONLY},
        .expectation = {.dialog = false},
    },
    {
        .name = "a white box without its arrow, box and arrow",
        .input = {.frame = box_without_arrow_frame, .white = WhiteDialogPart::BOX_AND_ARROW},
        .expectation = {.dialog = false},
    },
    {
        .name = "a white box without its arrow, box only",
        .input = {.frame = box_without_arrow_frame, .white = WhiteDialogPart::BOX_ONLY},
        .expectation = {.dialog = true},
    },
    {
        .name = "a white box on the console, box and arrow",
        .input = {
            .frame = []{ return console_frame("FlatWhiteDialogDetector/mac_restaurant_True.png"); },
            .white = WhiteDialogPart::BOX_AND_ARROW,
        },
        .expectation = {.dialog = true},
    },
    {
        .name = "a white box on the console, box only",
        .input = {
            .frame = []{ return console_frame("FlatWhiteDialogDetector/mac_restaurant_True.png"); },
            .white = WhiteDialogPart::BOX_ONLY,
        },
        .expectation = {.dialog = true},
    },
    {
        .name = "a white box on the console in French, box and arrow",
        .input = {
            .frame = []{ return console_frame("FlatWhiteDialogDetector/french_fossil_1_True.png"); },
            .white = WhiteDialogPart::BOX_AND_ARROW,
        },
        .expectation = {.dialog = true},
    },
    {
        .name = "a white box on the console with its arrow painted out, box and arrow",
        .input = {
            .frame = []{ return console_frame_without_arrow("FlatWhiteDialogDetector/mac_restaurant_True.png"); },
            .white = WhiteDialogPart::BOX_AND_ARROW,
        },
        .expectation = {.dialog = false},
    },
    {
        .name = "a white box on the console with its arrow painted out, box only",
        .input = {
            .frame = []{ return console_frame_without_arrow("FlatWhiteDialogDetector/mac_restaurant_True.png"); },
            .white = WhiteDialogPart::BOX_ONLY,
        },
        .expectation = {.dialog = true},
    },
    {
        .name = "a blue box on the console",
        .input = {
            .frame = []{ return console_frame("BlueDialogDetector/mac_cannotFastTravel_True.png"); },
            .white = WhiteDialogPart::BOX_AND_ARROW,
        },
        .expectation = {.dialog = true},
    },
};

class Test_StoryDialog : public UnitTest{
public:
    explicit Test_StoryDialog(const DialogCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - StoryDialogDetector: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        StoryDialogDetector detector({.white = m_test.input.white});
        const bool dialog = detector.detect(m_test.input.frame());
        if (dialog != m_test.expectation.dialog){
            return std::format("Expected {}, read {}.", m_test.expectation.dialog ? "a dialogue box" : "none", dialog ? "a dialogue box" : "none");
        }
        return true;
    }

private:
    const DialogCase& m_test;
};

} // namespace


void add_tests_AutoStoryDialogDetector(UnitTestDatabase& database){
    for (const DialogCase& test : DIALOG_CASES){
        database.add<Test_StoryDialog>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
