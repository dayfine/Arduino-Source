/*  PokemonLZA AutoStory - Zone gate reader Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Synthetic 1920x1080 frames with the pillars' lime drawn as bars, and
 *  console frames from CommandLineTests/PokemonLZA/AutoStoryDetectors/
 *  Zone1Gate/: the gate ahead from mid-street (1920x1080)
 *  and a walk's miss beside the zone's wall (kept resized
 *  to 640x360).
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonLZA/Inference/Map/PokemonLZA_DirectionArrowDetector.h"
#include "PokemonLZA_AutoStoryZoneGateReader.h"
#include "PokemonLZA_AutoStoryZoneGateReader_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr uint32_t STREET = 0xff606870;
//  The pillars' lime on the console.
constexpr uint32_t LIME = 0xffb4fa55;

//  A filled bar of the pillars' lime, in pixels of a 1920x1080 frame.
struct Bar{
    size_t x;
    size_t y;
    size_t width;
    size_t height;
};

ImageRGB32 frame_with(const std::vector<Bar>& bars){
    ImageRGB32 image(1920, 1080);
    image.fill(STREET);
    for (const Bar& bar : bars){
        for (size_t y = bar.y; y < bar.y + bar.height; y++){
            for (size_t x = bar.x; x < bar.x + bar.width; x++){
                image.pixel(x, y) = LIME;
            }
        }
    }
    return image;
}

//  Thin stripes `pitch` apart from `x` to `x + width`, as the zone's wall.
std::vector<Bar> stripes(size_t x, size_t width, size_t pitch){
    std::vector<Bar> bars;
    for (size_t at = x; at < x + width; at += pitch){
        bars.push_back({at, 300, 3, 500});
    }
    return bars;
}

//  A console frame of the Zone1Gate folder.
ImageRGB32 console_frame(std::string_view name){
    return ImageRGB32(std::format("{}PokemonLZA/AutoStoryDetectors/Zone1Gate/{}", UNIT_TEST_RESOURCE_PATH(), name));
}

std::string describe(const std::optional<ZoneGateSighting>& gate){
    return gate ? std::format("a gate at x = {:.3f} width {:.3f}", gate->x, gate->width) : std::string("no gate");
}


struct GateFrameCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        //  The gate; none: no gate. Within 0.01 on x, 0.02 on width.
        std::optional<ZoneGateSighting> gate;
        //  The minimap arrow's heading (DirectionArrowDetector), within 5
        //  degrees; none: not checked.
        std::optional<double> heading = std::nullopt;
    };
    Expectation expectation;
};

const GateFrameCase GATE_FRAME_CASES[] = {
    {
        .name = "two pillars in the middle",
        .input = {.frame = []{ return frame_with({{800, 220, 56, 200}, {1020, 220, 56, 200}}); }},
        .expectation = {.gate = ZoneGateSighting{.x = 0.489, .width = 0.144}},
    },
    {
        .name = "two pillars right of the middle",
        .input = {.frame = []{ return frame_with({{1300, 300, 40, 150}, {1460, 300, 40, 150}}); }},
        .expectation = {.gate = ZoneGateSighting{.x = 0.7292, .width = 0.104}},
    },
    {
        .name = "one pillar",
        .input = {.frame = []{ return frame_with({{800, 220, 56, 200}}); }},
        .expectation = {.gate = std::nullopt},
    },
    {
        .name = "two pillars touching",
        .input = {.frame = []{ return frame_with({{800, 220, 56, 200}, {858, 220, 56, 200}}); }},
        .expectation = {.gate = std::nullopt},
    },
    {
        .name = "two pillars too far apart",
        .input = {.frame = []{ return frame_with({{200, 220, 56, 200}, {1600, 220, 56, 200}}); }},
        .expectation = {.gate = std::nullopt},
    },
    {
        .name = "two pillars of different heights",
        .input = {.frame = []{ return frame_with({{800, 220, 56, 200}, {1020, 320, 56, 100}}); }},
        .expectation = {.gate = std::nullopt},
    },
    {
        .name = "two pillars with lime between them",
        .input = {.frame = []{ return frame_with({{800, 220, 56, 200}, {1020, 220, 56, 200}, {856, 220, 164, 60}}); }},
        .expectation = {.gate = std::nullopt},
    },
    {
        .name = "the wall's thin stripes",
        .input = {.frame = []{ return frame_with(stripes(900, 900, 24)); }},
        .expectation = {.gate = std::nullopt},
    },
    {.name = "no lime", .input = {.frame = []{ return frame_with({}); }}, .expectation = {.gate = std::nullopt}},
    {.name = "an empty frame", .input = {.frame = []{ return ImageRGB32(); }}, .expectation = {.gate = std::nullopt}},
    {.name = "a 1 x 1 frame", .input = {.frame = []{ return ImageRGB32(1, 1); }}, .expectation = {.gate = std::nullopt}},
    {
        .name = "console: Wild Zone 1's gate ahead from mid-street",
        .input = {.frame = []{ return console_frame("gate_ahead.png"); }},
        //  246: the street's heading toward the gate, which 03.06's
        //  to_zone_1_gate faces first.
        .expectation = {.gate = ZoneGateSighting{.x = 0.486, .width = 0.150}, .heading = 246},
    },
    {
        .name = "console: beside the zone's wall, the gate out of view (a walk's miss)",
        .input = {.frame = []{ return console_frame("beside_wall_640.png"); }},
        .expectation = {.gate = std::nullopt},
    },
};

class Test_ZoneGateFrame : public UnitTest{
public:
    explicit Test_ZoneGateFrame(const GateFrameCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Zone gate reader: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const ImageRGB32 frame = m_test.input.frame();
        const std::optional<ZoneGateSighting> gate = read_zone_gate(frame);
        logger.log(describe(gate));
        const std::optional<ZoneGateSighting>& want = m_test.expectation.gate;
        const bool same = gate.has_value() == want.has_value()
            && (!gate || (std::abs(gate->x - want->x) < 0.01 && std::abs(gate->width - want->width) < 0.02));
        if (!same){
            return std::format("Expected {}, read {}.", describe(want), describe(gate));
        }
        if (m_test.expectation.heading){
            return check_heading(frame, *m_test.expectation.heading);
        }
        return true;
    }

private:
    static UnitTestResult check_heading(const ImageRGB32& frame, double heading){
        DirectionArrowDetector arrow;
        if (!arrow.detect(frame)){
            return "No minimap arrow.";
        }
        double off = std::abs(arrow.detected_angle_deg() - heading);
        off = std::min(off, 360 - off);
        if (off > 5){
            return std::format("Expected the heading {:.0f} +/- 5, read {:.1f}.", heading, arrow.detected_angle_deg());
        }
        return true;
    }

    const GateFrameCase& m_test;
};

} // namespace


void add_tests_AutoStoryZoneGateReader(UnitTestDatabase& database){
    for (const GateFrameCase& test : GATE_FRAME_CASES){
        database.add<Test_ZoneGateFrame>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
