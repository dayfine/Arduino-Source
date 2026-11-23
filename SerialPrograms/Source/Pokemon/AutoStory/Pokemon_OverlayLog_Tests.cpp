/*  AutoStory Overlay Log Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_OverlayLog.h"
#include "Pokemon_OverlayLog_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

struct WrapOverlayCase{
    struct Input{
        std::string_view text = {};
        size_t width = OVERLAY_LOG_DEFAULT_WIDTH;
    };
    struct Expectation{
        std::vector<std::string> lines = {};
    };

    std::string_view name = {};
    Input input = {};
    Expectation expectation = {};
};

const WrapOverlayCase WRAP_OVERLAY_CASES[] = {
    {
        .name = "empty text produces no lines",
        .input = {.text = "", .width = 16},
        .expectation = {.lines = {}},
    },
    {
        .name = "short line fits without wrapping",
        .input = {.text = "Route 201 clear", .width = 34},
        .expectation = {.lines = {"Route 201 clear"}},
    },
    {
        .name = "long text wraps with two-space continuation indent",
        .input = {.text = "Walking north through Route 201 grass", .width = 18},
        .expectation = {.lines = {"Walking north", "  through Route", "  201 grass"}},
    },
    {
        .name = "single overlong word stays intact on its line",
        .input = {.text = "supercalifragilistic next", .width = 10},
        .expectation = {.lines = {"supercalifragilistic", "  next"}},
    },
};

class Test_WrapOverlayLog : public UnitTest{
public:
    explicit Test_WrapOverlayLog(const WrapOverlayCase& scenario)
        : UnitTest(std::format("Pokemon::OverlayLog - {}", scenario.name))
        , m_scenario(scenario)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::string> got = wrap_overlay_log_lines(
            m_scenario.input.text, m_scenario.input.width
        );
        if (got != m_scenario.expectation.lines){
            return std::format(
                "wrapped line mismatch: got {} lines, expected {}",
                got.size(), m_scenario.expectation.lines.size()
            );
        }
        return true;
    }

private:
    WrapOverlayCase m_scenario;
};

} // namespace


void add_tests_OverlayLog(UnitTestDatabase& database){
    for (const WrapOverlayCase& scenario : WRAP_OVERLAY_CASES){
        database.add<Test_WrapOverlayLog>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
