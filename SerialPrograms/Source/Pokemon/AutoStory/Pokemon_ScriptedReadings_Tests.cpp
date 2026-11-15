/*  Scripted Readings Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  ScriptedReadings on integer readings: which one shows after each wait,
 *  frames missing, the last one past the script's end, and the clock.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon_ScriptedReadings.h"
#include "Pokemon_ScriptedReadings_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

using namespace std::chrono_literals;

const WallClock T0 = WallClock() + std::chrono::hours(1);

struct ScriptedReadingsInput{
    std::vector<ScriptedReading<int>> script;
    //  Waited before each read.
    std::vector<Milliseconds> waits;
};

struct ScriptedReadingsExpectation{
    //  One per wait.
    std::vector<std::optional<int>> readings;
};

struct ScriptedReadingsCase{
    std::string name;
    ScriptedReadingsInput input;
    ScriptedReadingsExpectation expectation;
};

std::string reading_name(const std::optional<int>& reading){
    return reading ? std::to_string(*reading) : "no frame";
}

class Test_ScriptedReadings : public UnitTest{
public:
    explicit Test_ScriptedReadings(ScriptedReadingsCase test_case)
        : UnitTest("Pokemon::ScriptedReadings - " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        ScriptedReadings<int> source(m_case.input.script, T0);
        Milliseconds waited(0);
        for (size_t c = 0; c < m_case.input.waits.size(); c++){
            source.wait(m_case.input.waits[c]);
            waited += m_case.input.waits[c];
            const std::optional<int> reading = source.read();
            if (reading != m_case.expectation.readings[c]){
                return std::format(
                    "at {} ms: expected {}, read {}",
                    waited.count(), reading_name(m_case.expectation.readings[c]), reading_name(reading)
                );
            }
            if (source.now() != T0 + waited || source.elapsed() != waited){
                return std::format("at {} ms: the clock is at {} ms", waited.count(), source.elapsed().count());
            }
        }
        return true;
    }

private:
    ScriptedReadingsCase m_case;
};

const ScriptedReadingsCase SCRIPTED_READINGS_CASES[] = {
    {
        .name = "each reading for its time",
        .input = {.script = {{100ms, 1}, {200ms, 2}}, .waits = {0ms, 99ms, 1ms, 199ms}},
        .expectation = {.readings = {1, 1, 2, 2}},
    },
    {
        .name = "the last reading after the script ends",
        .input = {.script = {{100ms, 1}, {100ms, 2}}, .waits = {200ms, 60000ms}},
        .expectation = {.readings = {2, 2}},
    },
    {
        .name = "a step without a reading is no frame",
        .input = {.script = {{100ms, 1}, {100ms, std::nullopt}, {100ms, 3}}, .waits = {0ms, 100ms, 100ms}},
        .expectation = {.readings = {1, std::nullopt, 3}},
    },
    {
        .name = "an empty script reads no frame",
        .input = {.script = {}, .waits = {0ms, 1000ms}},
        .expectation = {.readings = {std::nullopt, std::nullopt}},
    },
    {
        .name = "reading again without a wait reads the same",
        .input = {.script = {{100ms, 1}, {100ms, 2}}, .waits = {50ms, 0ms, 0ms}},
        .expectation = {.readings = {1, 1, 1}},
    },
};

} // namespace


void add_tests_ScriptedReadings(UnitTestDatabase& database){
    for (const ScriptedReadingsCase& test_case : SCRIPTED_READINGS_CASES){
        database.add<Test_ScriptedReadings>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
