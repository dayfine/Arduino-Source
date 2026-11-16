/*  PokemonLZA AutoStory - Objective marker reader Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Synthetic 1920x1080 frames: the marker's orange drawn as squares and
 *  bars where the reader must take or skip them.
 *
 */

#include <cmath>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_AutoStoryObjectiveMarkerReader.h"
#include "PokemonLZA_AutoStoryObjectiveMarkerReader_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr uint32_t BACKGROUND = 0xff304030;
//  The hexagon's orange on console B.
constexpr uint32_t ORANGE = 0xfff5af28;

//  A filled rectangle of the marker's orange, in pixels of a 1920x1080
//  frame.
struct Patch{
    size_t x;
    size_t y;
    size_t width;
    size_t height;
};

ImageRGB32 frame_with(const std::vector<Patch>& patches){
    ImageRGB32 image(1920, 1080);
    image.fill(BACKGROUND);
    for (const Patch& patch : patches){
        for (size_t y = patch.y; y < patch.y + patch.height; y++){
            for (size_t x = patch.x; x < patch.x + patch.width; x++){
                image.pixel(x, y) = ORANGE;
            }
        }
    }
    return image;
}

std::string describe_x(std::optional<double> x){
    return x ? std::format("a marker at x = {:.3f}", *x) : std::string("no marker");
}


struct SyntheticCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        //  The marker's middle as a share of the width; none: no marker.
        std::optional<double> x;
    };
    Expectation expectation;
};

const SyntheticCase SYNTHETIC_CASES[] = {
    {
        .name = "a marker in the middle",
        .input = {.frame = []{ return frame_with({{930, 500, 60, 60}}); }},
        .expectation = {.x = 0.5},
    },
    {
        .name = "a marker right of the middle",
        .input = {.frame = []{ return frame_with({{1506, 300, 60, 60}}); }},
        .expectation = {.x = 0.8},
    },
    {
        .name = "the larger of two markers",
        .input = {.frame = []{ return frame_with({{400, 400, 40, 40}, {1506, 400, 60, 60}}); }},
        .expectation = {.x = 0.8},
    },
    {
        .name = "a marker beside the minimap's own",
        .input = {.frame = []{ return frame_with({{150, 150, 60, 60}, {930, 500, 40, 40}}); }},
        .expectation = {.x = 0.5},
    },
    {
        .name = "the minimap's marker only",
        .input = {.frame = []{ return frame_with({{150, 150, 60, 60}}); }},
        .expectation = {.x = std::nullopt},
    },
    {
        .name = "orange in the party bar only",
        .input = {.frame = []{ return frame_with({{930, 800, 60, 60}}); }},
        .expectation = {.x = std::nullopt},
    },
    {
        .name = "an orange blob too large",
        .input = {.frame = []{ return frame_with({{900, 400, 100, 100}}); }},
        .expectation = {.x = std::nullopt},
    },
    {
        .name = "an orange bar too wide",
        .input = {.frame = []{ return frame_with({{900, 400, 90, 30}}); }},
        .expectation = {.x = std::nullopt},
    },
    {
        .name = "an orange speck too small",
        .input = {.frame = []{ return frame_with({{930, 500, 15, 15}}); }},
        .expectation = {.x = std::nullopt},
    },
    {.name = "no orange", .input = {.frame = []{ return frame_with({}); }}, .expectation = {.x = std::nullopt}},
    {.name = "an empty frame", .input = {.frame = []{ return ImageRGB32(); }}, .expectation = {.x = std::nullopt}},
    {.name = "a 1 x 1 frame", .input = {.frame = []{ return ImageRGB32(1, 1); }}, .expectation = {.x = std::nullopt}},
};

class Test_ObjectiveMarkerSynthetic : public UnitTest{
public:
    explicit Test_ObjectiveMarkerSynthetic(const SyntheticCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Objective marker reader: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::optional<ObjectiveMarker> marker = read_objective_marker(logger, m_test.input.frame());
        const std::optional<double> x = marker ? std::optional<double>(marker->x) : std::nullopt;
        const bool same = x.has_value() == m_test.expectation.x.has_value()
            && (!x || std::abs(*x - *m_test.expectation.x) < 0.01);
        if (!same){
            return std::format("Expected {}, read {}.", describe_x(m_test.expectation.x), describe_x(x));
        }
        return true;
    }

private:
    const SyntheticCase& m_test;
};

} // namespace


void add_tests_AutoStoryObjectiveMarkerReader(UnitTestDatabase& database){
    for (const SyntheticCase& test : SYNTHETIC_CASES){
        database.add<Test_ObjectiveMarkerSynthetic>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
