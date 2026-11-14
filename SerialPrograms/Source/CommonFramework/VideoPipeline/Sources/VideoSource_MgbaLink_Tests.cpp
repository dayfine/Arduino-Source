/*  Video Source (mGBA over MgbaLink) Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <optional>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoSource_MgbaLink.h"
#include "VideoSource_MgbaLink_Tests.h"

namespace PokemonAutomation{

namespace{


//  A 240x160 frame whose pixels name their own coordinates.
ImageRGB32 make_gba_frame(){
    ImageRGB32 frame(240, 160);
    for (size_t y = 0; y < 160; y++){
        for (size_t x = 0; x < 240; x++){
            frame.pixel(x, y) = 0xff000000 | (uint32_t)(x << 8) | (uint32_t)y;
        }
    }
    return frame;
}


UnitTestResult test_compose_geometry(){
    struct Point{
        size_t x;
        size_t y;
    };
    struct Case{
        std::string name;
        Point canvas;
        std::optional<Point> expected_source;   //  nullopt: black border.
    };
    const Case CASES[] = {
        {"box top-left", {180, 5}, Point{0, 0}},
        {"box bottom-right", {1739, 1044}, Point{239, 159}},
        {"left of the box", {179, 500}, std::nullopt},
        {"right of the box", {1740, 500}, std::nullopt},
        {"above the box", {960, 4}, std::nullopt},
        {"below the box", {960, 1045}, std::nullopt},
        //  6.5x: source columns alternate 6 and 7 canvas pixels.
        {"last pixel of column 0", {185, 5}, Point{0, 0}},
        {"first pixel of column 1", {186, 5}, Point{1, 0}},
        {"last pixel of column 1", {192, 5}, Point{1, 0}},
        {"first pixel of column 2", {193, 5}, Point{2, 0}},
        {"first pixel of row 1", {180, 11}, Point{0, 1}},
        {"centre", {960, 525}, Point{120, 80}},
    };
    ImageRGB32 canvas = compose_mgba_frame(make_gba_frame());
    if (canvas.width() != 1920 || canvas.height() != 1080){
        return std::format("canvas is {}x{}, expected 1920x1080", canvas.width(), canvas.height());
    }
    for (const Case& c : CASES){
        uint32_t expected = c.expected_source
            ? 0xff000000 | (uint32_t)(c.expected_source->x << 8) | (uint32_t)c.expected_source->y
            : 0xff000000;
        uint32_t actual = canvas.pixel(c.canvas.x, c.canvas.y);
        if (actual != expected){
            return std::format("{}: pixel {:08x}, expected {:08x}", c.name, actual, expected);
        }
    }
    return true;
}


UnitTestResult test_parse_address(){
    struct Expectation{
        std::string host;
        uint16_t port;
    };
    struct Case{
        std::string name;
        std::string input;
        std::optional<Expectation> expectation;
    };
    const Case CASES[] = {
        {"default bridge", "127.0.0.1:47801", Expectation{"127.0.0.1", 47801}},
        {"host name", "localhost:47802", Expectation{"localhost", 47802}},
        {"no port", "127.0.0.1", std::nullopt},
        {"empty port", "127.0.0.1:", std::nullopt},
        {"port 0", "127.0.0.1:0", std::nullopt},
        {"port too large", "127.0.0.1:70000", std::nullopt},
        {"no host", ":47801", std::nullopt},
    };
    for (const Case& c : CASES){
        std::optional<std::pair<std::string, uint16_t>> address = parse_mgba_address(c.input);
        if (address.has_value() != c.expectation.has_value()){
            return c.name + ": parsed=" + (address ? "true" : "false");
        }
        if (address && (address->first != c.expectation->host || address->second != c.expectation->port)){
            return c.name + ": wrong host or port.";
        }
    }
    return true;
}



class Test_VideoSource_MgbaLink : public UnitTest{
public:
    Test_VideoSource_MgbaLink(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("CommonFramework::VideoSource_MgbaLink - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_body();
    }

private:
    std::function<UnitTestResult()> m_body;
};

}


void add_tests_VideoSource_MgbaLink(UnitTestDatabase& database){
    database.add<Test_VideoSource_MgbaLink>("compose geometry", test_compose_geometry);
    database.add<Test_VideoSource_MgbaLink>("address", test_parse_address);
}



}
