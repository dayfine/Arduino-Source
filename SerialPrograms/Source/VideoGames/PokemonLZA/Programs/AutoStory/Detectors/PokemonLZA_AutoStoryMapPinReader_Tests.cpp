/*  PokemonLZA AutoStory - Map pin reader Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Synthetic 1920x1080 frames with the arrow's blue drawn as a square,
 *  and console frames of the full map from CommandLineTests/PokemonLZA/
 *  AutoStoryDetectors/Zone1Map/ (console B, 2026-11-16): stuck at the
 *  zone's north fence, and at the patrol's stop 10 (marker read ~87),
 *  the arrow partly under the zone's badge.
 *
 */

#include <cmath>
#include <format>
#include <initializer_list>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_AutoStoryMapPinReader.h"
#include "PokemonLZA_AutoStoryMapPinReader_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The zone's green and the arrow's and cone's blues on console B.
constexpr uint32_t ZONE_GREEN = 0xff98efad;
constexpr uint32_t ARROW_BLUE = 0xff00a0ff;
constexpr uint32_t CONE_BLUE = 0xff5ec9ea;

//  A `side` px square centred at (x, y) in map coordinates, on a 1920x1080 frame.
struct Square{
    double x;
    double y;
    size_t side;
    uint32_t colour;
};

ImageRGB32 map_with(std::initializer_list<Square> squares){
    ImageRGB32 image(1920, 1080);
    image.fill(ZONE_GREEN);
    for (const Square& square : squares){
        const size_t cx = (size_t)(square.x * 2.4);
        const size_t cy = (size_t)(square.y * 2.4);
        for (size_t y = cy - square.side / 2; y < cy + square.side / 2; y++){
            for (size_t x = cx - square.side / 2; x < cx + square.side / 2; x++){
                image.pixel(x, y) = square.colour;
            }
        }
    }
    return image;
}

ImageRGB32 console_frame(std::string_view name){
    return ImageRGB32(std::format("{}PokemonLZA/AutoStoryDetectors/Zone1Map/{}", UNIT_TEST_RESOURCE_PATH(), name));
}

std::string describe(const std::optional<MapPoint>& pin){
    return pin ? std::format("a pin at ({:.1f}, {:.1f})", pin->x, pin->y) : std::string("no pin");
}


struct MapPinCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        //  The pin, within 2 on each axis; none: no pin.
        std::optional<MapPoint> pin;
    };
    Expectation expectation;
};

const MapPinCase MAP_PIN_CASES[] = {
    {
        .name = "an arrow in the zone",
        .input = {.frame = []{ return map_with({{.x = 400, .y = 230, .side = 24, .colour = ARROW_BLUE}}); }},
        .expectation = {.pin = MapPoint{.x = 400, .y = 230}},
    },
    {
        .name = "the arrow with the camera's cone beside it",
        .input = {.frame = []{ return map_with({
            {.x = 350, .y = 215, .side = 40, .colour = CONE_BLUE},
            {.x = 370, .y = 230, .side = 24, .colour = ARROW_BLUE},
        }); }},
        .expectation = {.pin = MapPoint{.x = 370, .y = 230}},
    },
    {
        .name = "the cone only",
        .input = {.frame = []{ return map_with({{.x = 350, .y = 215, .side = 40, .colour = CONE_BLUE}}); }},
        .expectation = {.pin = std::nullopt},
    },
    {
        .name = "the arrow's blue outside the zone's window",
        .input = {.frame = []{ return map_with({{.x = 100, .y = 100, .side = 24, .colour = ARROW_BLUE}}); }},
        .expectation = {.pin = std::nullopt},
    },
    {
        .name = "a speck of the arrow's blue",
        .input = {.frame = []{ return map_with({{.x = 400, .y = 230, .side = 6, .colour = ARROW_BLUE}}); }},
        .expectation = {.pin = std::nullopt},
    },
    {.name = "no arrow", .input = {.frame = []{ return map_with({}); }}, .expectation = {.pin = std::nullopt}},
    {.name = "an empty frame", .input = {.frame = []{ return ImageRGB32(); }}, .expectation = {.pin = std::nullopt}},
    {
        .name = "console: stuck at the zone's north fence",
        .input = {.frame = []{ return console_frame("at_north_fence.png"); }},
        .expectation = {.pin = MapPoint{.x = 358.9, .y = 227.3}},
    },
    {
        .name = "console: the patrol's stop 10, the arrow partly under the zone's badge",
        .input = {.frame = []{ return console_frame("after_try6.png"); }},
        .expectation = {.pin = MapPoint{.x = 415.4, .y = 230.4}},
    },
};

class Test_MapPin : public UnitTest{
public:
    explicit Test_MapPin(const MapPinCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Map pin reader: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::optional<MapPoint> pin = read_map_pin(m_test.input.frame());
        logger.log(describe(pin));
        const std::optional<MapPoint>& want = m_test.expectation.pin;
        const bool same = pin.has_value() == want.has_value()
            && (!pin || (std::abs(pin->x - want->x) <= 2 && std::abs(pin->y - want->y) <= 2));
        if (!same){
            return std::format("Expected {}, read {}.", describe(want), describe(pin));
        }
        return true;
    }

private:
    const MapPinCase& m_test;
};

} // namespace


void add_tests_AutoStoryMapPinReader(UnitTestDatabase& database){
    for (const MapPinCase& test : MAP_PIN_CASES){
        database.add<Test_MapPin>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
