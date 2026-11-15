/*  Video Source (mGBA over MgbaLink) Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include "Common/Cpp/Exceptions.h"
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

struct Point{
    size_t x;
    size_t y;
};


//  Which GBA pixel lands on a canvas pixel of the composed 1920x1080 frame.
struct ComposeInput{
    Point canvas;
};
struct ComposeExpectation{
    std::optional<Point> gba_pixel;     //  nullopt: black border.
};
struct ComposeCase{
    std::string name;
    ComposeInput input;
    ComposeExpectation expectation;
};

class Test_Compose : public UnitTest{
public:
    Test_Compose(ComposeCase test_case)
        : UnitTest(std::format("CommonFramework::VideoSource_MgbaLink - compose - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        ImageRGB32 canvas = compose_mgba_frame(make_gba_frame());
        if (canvas.width() != 1920 || canvas.height() != 1080){
            return std::format("canvas is {}x{}, expected 1920x1080", canvas.width(), canvas.height());
        }
        const std::optional<Point>& gba = m_case.expectation.gba_pixel;
        uint32_t expected = gba ? 0xff000000 | (uint32_t)(gba->x << 8) | (uint32_t)gba->y : 0xff000000;
        uint32_t actual = canvas.pixel(m_case.input.canvas.x, m_case.input.canvas.y);
        if (actual != expected){
            return std::format("pixel {:08x}, expected {:08x}", actual, expected);
        }
        return true;
    }

private:
    ComposeCase m_case;
};


//  "host:port" parsing for the descriptor.
struct AddressInput{
    std::string address;
};
struct AddressExpectation{
    std::optional<std::pair<std::string, uint16_t>> host_and_port;
};
struct AddressCase{
    std::string name;
    AddressInput input;
    AddressExpectation expectation;
};

class Test_Address : public UnitTest{
public:
    Test_Address(AddressCase test_case)
        : UnitTest(std::format("CommonFramework::VideoSource_MgbaLink - address - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::optional<std::pair<std::string, uint16_t>> parsed = parse_mgba_address(m_case.input.address);
        if (parsed != m_case.expectation.host_and_port){
            return std::format(
                "parsed {}, expected {}",
                describe(parsed), describe(m_case.expectation.host_and_port)
            );
        }
        return true;
    }

private:
    static std::string describe(const std::optional<std::pair<std::string, uint16_t>>& host_and_port){
        return host_and_port
            ? std::format("<{}> port {}", host_and_port->first, host_and_port->second)
            : std::string("nothing");
    }

    AddressCase m_case;
};


//  compose_mgba_frame() refuses a box that does not fit the canvas.
struct GeometryInput{
    MgbaComposeGeometry geometry;
};
struct GeometryExpectation{
    bool accepted = false;
};
struct GeometryCase{
    std::string name;
    GeometryInput input;
    GeometryExpectation expectation;
};

class Test_Geometry : public UnitTest{
public:
    Test_Geometry(GeometryCase test_case)
        : UnitTest(std::format("CommonFramework::VideoSource_MgbaLink - geometry - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        bool accepted = true;
        try{
            compose_mgba_frame(make_gba_frame(), m_case.input.geometry);
        }catch (InternalProgramError&){
            accepted = false;
        }
        if (accepted != m_case.expectation.accepted){
            return std::format("accepted={}, expected {}", accepted, m_case.expectation.accepted);
        }
        return true;
    }

private:
    GeometryCase m_case;
};

}  // namespace


void add_tests_VideoSource_MgbaLink(UnitTestDatabase& database){
    //  GAME_BOX at 1080p is x = 180..1739, y = 5..1044; 6.5x, so source
    //  columns alternate 6 and 7 canvas pixels.
    const ComposeCase COMPOSE_CASES[] = {
        {.name = "box top-left", .input = {{180, 5}}, .expectation = {Point{0, 0}}},
        {.name = "box bottom-right", .input = {{1739, 1044}}, .expectation = {Point{239, 159}}},
        {.name = "left of the box", .input = {{179, 500}}, .expectation = {std::nullopt}},
        {.name = "right of the box", .input = {{1740, 500}}, .expectation = {std::nullopt}},
        {.name = "above the box", .input = {{960, 4}}, .expectation = {std::nullopt}},
        {.name = "below the box", .input = {{960, 1045}}, .expectation = {std::nullopt}},
        {.name = "last pixel of column 0", .input = {{185, 5}}, .expectation = {Point{0, 0}}},
        {.name = "first pixel of column 1", .input = {{186, 5}}, .expectation = {Point{1, 0}}},
        {.name = "last pixel of column 1", .input = {{192, 5}}, .expectation = {Point{1, 0}}},
        {.name = "first pixel of column 2", .input = {{193, 5}}, .expectation = {Point{2, 0}}},
        {.name = "first pixel of row 1", .input = {{180, 11}}, .expectation = {Point{0, 1}}},
        {.name = "centre", .input = {{960, 525}}, .expectation = {Point{120, 80}}},
    };
    for (const ComposeCase& test_case : COMPOSE_CASES){
        database.add<Test_Compose>(test_case);
    }

    const AddressCase ADDRESS_CASES[] = {
        {.name = "default bridge", .input = {"127.0.0.1:47801"}, .expectation = {std::make_pair(std::string("127.0.0.1"), uint16_t(47801))}},
        {.name = "host name", .input = {"localhost:47802"}, .expectation = {std::make_pair(std::string("localhost"), uint16_t(47802))}},
        {.name = "no port", .input = {"127.0.0.1"}, .expectation = {std::nullopt}},
        {.name = "empty port", .input = {"127.0.0.1:"}, .expectation = {std::nullopt}},
        {.name = "port 0", .input = {"127.0.0.1:0"}, .expectation = {std::nullopt}},
        {.name = "port too large", .input = {"127.0.0.1:70000"}, .expectation = {std::nullopt}},
        {.name = "no host", .input = {":47801"}, .expectation = {std::nullopt}},
        {.name = "remote host refused", .input = {"192.168.1.20:47801"}, .expectation = {std::nullopt}},
    };
    for (const AddressCase& test_case : ADDRESS_CASES){
        database.add<Test_Address>(test_case);
    }

    const GeometryCase GEOMETRY_CASES[] = {
        {.name = "GAME_BOX at 1080p fits", .input = {MgbaComposeGeometry()}, .expectation = {true}},
        {.name = "box fills the canvas", .input = {{.canvas_width = 240, .canvas_height = 160, .box_x = 0, .box_y = 0, .box_width = 240, .box_height = 160}}, .expectation = {true}},
        {.name = "box past the right edge", .input = {{.box_x = 400}}, .expectation = {false}},
        {.name = "box past the bottom edge", .input = {{.box_y = 50}}, .expectation = {false}},
    };
    for (const GeometryCase& test_case : GEOMETRY_CASES){
        database.add<Test_Geometry>(test_case);
    }
}



}  // namespace PokemonAutomation
