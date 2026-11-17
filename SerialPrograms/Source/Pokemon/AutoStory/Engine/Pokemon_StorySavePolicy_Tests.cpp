/*  Story Save Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The autosave policy's answer at a checkpoint's end, from where the
 *  autosave was seen and what the plan's rows expected, with its log line.
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryFakeBackend.h"
#include "Pokemon_StorySavePolicy.h"
#include "Pokemon_ZonePose.h"
#include "Pokemon_StorySavePolicy_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


struct AutosavePolicyCase{
    std::string name;
    struct Input{
        AutosaveSighting sighting = {};
        StoryRows rows = {};
    };
    Input input;
    struct Expectation{
        //  "saved", or "not saved: <detail>".
        std::string answer;
        //  The policy's log lines, joined by "; ".
        std::string log = {};
    };
    Expectation expectation;
};

//  Row 01.00: steps talk, walk, door.
StoryCheckpoint test_row(){
    StoryCheckpoint row;
    row.id = "01.00";
    for (const char* step : {"talk", "walk", "door"}){
        row.steps.push_back(StoryStep{.name = step});
    }
    return row;
}

const AutosavePolicyCase CASES[] = {
    {
        .name = "an autosave in the last step is a save",
        .input = {.sighting = {.first_step = "door", .at_end = true}},
        .expectation = {.answer = "saved", .log = "[Save] 01.00: the game autosaved (seen)"},
    },
    {
        .name = "an autosave before the last step is not a save",
        .input = {.sighting = {.first_step = "talk"}},
        .expectation = {
            .answer = "not saved: autosaved before the last step",
            .log = "[Save] 01.00: the game autosaved in talk, before walk: not saved, a reload would skip walk",
        },
    },
    {
        .name = "no autosave on an unverified row",
        .input = {.rows = {.unverified_autosaves = {"01.00"}}},
        .expectation = {.answer = "not saved: save unverified"},
    },
    {
        .name = "no autosave on an autosave row",
        .input = {.rows = {.autosaves = {"01.00"}}},
        .expectation = {.answer = "not saved: the plan's autosave was not seen"},
    },
    {
        .name = "no autosave on a row with no save",
        .input = {},
        .expectation = {.answer = "not saved: no autosave seen"},
    },
};

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += text.empty() ? line : "; " + line;
    }
    return text;
}


class AutosavePolicyTest : public UnitTest{
public:
    explicit AutosavePolicyTest(const AutosavePolicyCase& test)
        : UnitTest(std::format("Pokemon::StorySavePolicy - autosave: {}", test.name))
        , m_test(test)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        FakeStoryBackend<ZonePose> backend({}, scope);
        const AutosaveSighting sighting = m_test.input.sighting;
        const StorySavePolicy<ZonePose> policy = autosave_policy<ZonePose>(
            m_test.input.rows, [sighting](const StoryCheckpoint&){ return sighting; }
        );
        const Outcome outcome = policy(backend, test_row());
        const std::string answer = outcome.succeeded()
            ? "saved"
            : outcome.failure == NOT_SAVED ? std::format("not saved: {}", outcome.detail) : to_string(outcome);
        if (answer != m_test.expectation.answer){
            return std::format("expected \"{}\", got \"{}\"", m_test.expectation.answer, answer);
        }
        if (joined(backend.lines()) != m_test.expectation.log){
            return std::format("expected the log \"{}\", got \"{}\"", m_test.expectation.log, joined(backend.lines()));
        }
        return true;
    }

private:
    const AutosavePolicyCase& m_test;
};


} // namespace


void add_tests_StorySavePolicy(UnitTestDatabase& database){
    for (const AutosavePolicyCase& test : CASES){
        database.add<AutosavePolicyTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
