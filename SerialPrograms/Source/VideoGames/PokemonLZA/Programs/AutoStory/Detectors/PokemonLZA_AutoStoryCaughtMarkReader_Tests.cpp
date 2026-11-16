/*  PokemonLZA AutoStory - Caught mark reader Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Synthetic 1920x1080 frames of the lock-on name plate.
 *
 */

#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_AutoStoryCaughtMarkReader.h"
#include "PokemonLZA_AutoStoryCaughtMarkReader_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr size_t WIDTH = 1920;
constexpr size_t HEIGHT = 1080;
constexpr uint32_t WHITE = 0xffffffff;

//  Where the drawn frames put the name plate's parts, in 1080p pixels.
const ImagePixelBox PLATE_BAR_BOX(950, 515, 1058, 533);
const ImagePixelBox PLAYER_BAR_BOX(990, 560, 1053, 572);
const ImagePixelBox GENDER_MARK_BOX(1020, 430, 1044, 454);
const ImagePixelBox GENDER_MARK_HOLE_BOX(1028, 438, 1036, 446);
const ImagePixelBox BALL_MARK_TOP_BOX(1056, 430, 1080, 442);
const ImagePixelBox BALL_MARK_BOTTOM_BOX(1056, 442, 1080, 454);

void fill_box(const ImagePixelBox& box, uint32_t color, ImageRGB32& image){
    for (size_t y = box.min_y; y < box.max_y; y++){
        for (size_t x = box.min_x; x < box.max_x; x++){
            image.pixel(x, y) = color;
        }
    }
}

ImageRGB32 solid_frame(uint32_t color){
    ImageRGB32 image(WIDTH, HEIGHT);
    image.fill(color);
    return image;
}


//  The lock-on name plate's HP bar (lock_target_caught()'s), drawn as on
//  console B's 03.08 frames (2026-11-14, 640 px, scaled here to 1920):
//  the bar about 45, 198, 115 and 108 x 18 under the target's name; the
//  bright grass around it (68, 160, 80, Bunnelby's plate at 60) and the
//  player's own HP bar (63 x 12, over the head while a wild Pokemon
//  attacks). Above the bar's right end, the gender mark (an orange disk
//  with a white sign, about 255, 118, 68) and, for a species already
//  caught, a Poke Ball (pinkish red over white, about 255, 107, 110):
//  Scatterbug's plate shows only the first, Bunnelby's both.
constexpr uint32_t PLATE_BAR_GREEN = 0xff2dc673;
constexpr uint32_t GENDER_ORANGE = 0xffff7644;
constexpr uint32_t BALL_RED = 0xffff6b6e;
constexpr uint32_t BRIGHT_GRASS = 0xff44a050;
constexpr uint32_t DARK_GROUND = 0xff303030;

//  What plate_frame() draws besides `ground`.
struct PlateDrawing{
    bool plate_bar = false;     //  at x 950, y 515
    bool player_bar = false;    //  at x 990, y 560
    bool gender_mark = false;   //  at x 1020, y 430
    bool ball_mark = false;     //  at x 1056, y 430
};

ImageRGB32 plate_frame(uint32_t ground, PlateDrawing drawing){
    ImageRGB32 image = solid_frame(ground);
    if (drawing.plate_bar){
        fill_box(PLATE_BAR_BOX, PLATE_BAR_GREEN, image);
    }
    if (drawing.player_bar){
        fill_box(PLAYER_BAR_BOX, PLATE_BAR_GREEN, image);
    }
    if (drawing.gender_mark){
        fill_box(GENDER_MARK_BOX, GENDER_ORANGE, image);
        fill_box(GENDER_MARK_HOLE_BOX, WHITE, image);
    }
    if (drawing.ball_mark){
        fill_box(BALL_MARK_TOP_BOX, BALL_RED, image);
        fill_box(BALL_MARK_BOTTOM_BOX, WHITE, image);
    }
    return image;
}

struct PlateFrameCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        //  lock_target_caught(); none: no plate.
        std::optional<bool> caught;
    };
    Expectation expectation;
};

const PlateFrameCase PLATE_FRAME_CASES[] = {
    {
        .name = "the plate's bar on dark ground",
        .input = {.frame = []{ return plate_frame(DARK_GROUND, {.plate_bar = true}); }},
        .expectation = {.caught = false},
    },
    {
        .name = "the plate's bar on the bright grass it touches",
        .input = {.frame = []{ return plate_frame(BRIGHT_GRASS, {.plate_bar = true}); }},
        .expectation = {.caught = false},
    },
    {
        .name = "the plate's bar and the player's HP bar",
        .input = {.frame = []{ return plate_frame(BRIGHT_GRASS, {.plate_bar = true, .player_bar = true}); }},
        .expectation = {.caught = false},
    },
    {
        .name = "a plate with the gender mark only (Scatterbug, not caught)",
        .input = {.frame = []{ return plate_frame(DARK_GROUND, {.plate_bar = true, .gender_mark = true}); }},
        .expectation = {.caught = false},
    },
    {
        .name = "a plate with the gender mark and the Poke Ball (Bunnelby, caught)",
        .input = {.frame = []{ return plate_frame(DARK_GROUND, {.plate_bar = true, .gender_mark = true, .ball_mark = true}); }},
        .expectation = {.caught = true},
    },
    {
        .name = "the player's HP bar alone",
        .input = {.frame = []{ return plate_frame(DARK_GROUND, {.player_bar = true}); }},
        .expectation = {.caught = std::nullopt},
    },
    {
        .name = "bright grass alone",
        .input = {.frame = []{ return plate_frame(BRIGHT_GRASS, {}); }},
        .expectation = {.caught = std::nullopt},
    },
    {
        .name = "a frame all in the bar's green",
        .input = {.frame = []{ return solid_frame(PLATE_BAR_GREEN); }},
        .expectation = {.caught = std::nullopt},
    },
    {
        .name = "an empty frame",
        .input = {.frame = []{ return ImageRGB32(); }},
        .expectation = {.caught = std::nullopt},
    },
};

std::string describe(const std::optional<bool>& caught){
    if (!caught){
        return "no plate";
    }
    return *caught ? "a plate with the caught mark" : "a plate with no caught mark";
}

class Test_PlateFrame : public UnitTest{
public:
    explicit Test_PlateFrame(const PlateFrameCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Caught mark reader: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<bool> caught = lock_target_caught(m_test.input.frame());
        if (caught != m_test.expectation.caught){
            return std::format("Expected {}, read {}.", describe(m_test.expectation.caught), describe(caught));
        }
        return true;
    }

private:
    const PlateFrameCase& m_test;
};

} // namespace


void add_tests_AutoStoryCaughtMarkReader(UnitTestDatabase& database){
    for (const PlateFrameCase& test : PLATE_FRAME_CASES){
        database.add<Test_PlateFrame>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
