/*  BDSP AutoStory - Indoors tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonBDSP_AutoStoryIndoors.h"
#include "PokemonBDSP_AutoStoryIndoors_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

struct IndoorsCase{
    struct Input{
        uint32_t fill;
        double dark_bottom;     //  how much of the screen's height, from the bottom, is black
    };
    struct Expectation{
        bool indoors;
    };
    std::string name;
    Input input;
    Expectation expectation;
};

class Test_Indoors : public UnitTest{
public:
    explicit Test_Indoors(IndoorsCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Indoors: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        ImageRGB32 screen(1920, 1080);
        const size_t dark_from = (size_t)(1080 * (1 - m_case.input.dark_bottom));
        for (size_t y = 0; y < 1080; y++){
            for (size_t x = 0; x < 1920; x++){
                screen.pixel(x, y) = y >= dark_from ? 0xff000000 : m_case.input.fill;
            }
        }
        const bool inside = indoors(screen);
        if (inside != m_case.expectation.indoors){
            return std::format("expected {}, read {}.",
                m_case.expectation.indoors ? "inside" : "outside", inside ? "inside" : "outside");
        }
        return true;
    }

private:
    IndoorsCase m_case;
};

constexpr uint32_t ORANGE = 0xffe0a040;
constexpr uint32_t GRASS = 0xff60b040;

const IndoorsCase INDOORS_CASES[] = {
    {.name = "evening outside, orange all over", .input = {.fill = ORANGE, .dark_bottom = 0}, .expectation = {.indoors = false}},
    {.name = "a room: orange, black below its floor", .input = {.fill = ORANGE, .dark_bottom = 0.10}, .expectation = {.indoors = true}},
    //  lab-B 00.01 above_the_door (Japanese new game): PlayerHouse1F mid-room
    //  showed 10.6..14.4% black on its bottom edge and read as outside.
    {.name = "a big room: orange, a narrow black band below its floor", .input = {.fill = ORANGE, .dark_bottom = 0.015}, .expectation = {.indoors = true}},
    {.name = "a fade: black all over", .input = {.fill = 0xff000000, .dark_bottom = 0}, .expectation = {.indoors = false}},
    {.name = "orange with a thin shadow at the bottom", .input = {.fill = ORANGE, .dark_bottom = 0.01}, .expectation = {.indoors = false}},
    {.name = "daytime grass", .input = {.fill = GRASS, .dark_bottom = 0.10}, .expectation = {.indoors = false}},
};

} // namespace


void add_tests_AutoStory_Indoors(UnitTestDatabase& database){
    for (const IndoorsCase& item : INDOORS_CASES){
        database.add<Test_Indoors>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
