/*  Story Action Arg Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checking a step's args against an action's spec and filling in its
 *  fallbacks. Built standalone (PA_STORY_DATA_STANDALONE), it runs without
 *  the application.
 *
 */

#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon_StoryActionArg.h"
#ifndef PA_STORY_DATA_STANDALONE
#include "Pokemon_StoryActionArg_Tests.h"
#endif

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  The args a Mart step takes, as a game declares them.
const StoryActionSpec& mart_spec(){
    static const StoryActionSpec spec{.kind = StoryStepKind::GAME, .name = "mart", .args = {
        {.key = "map", .type = StoryArgType::WORD, .required = true, .fallback = std::nullopt, .why = "the Mart's map"},
        {.key = "budget", .type = StoryArgType::INTEGER, .required = true, .fallback = std::nullopt, .why = ""},
        {.key = "floor", .type = StoryArgType::INTEGER, .required = false, .fallback = int64_t(0), .why = ""},
        {.key = "if_present", .type = StoryArgType::BOOLEAN, .required = false, .fallback = std::nullopt, .why = ""},
    }};
    return spec;
}

//  Args checked against the Mart's spec, then filled in.
struct ArgsInput{
    std::string_view name;
    StoryActionArgs args;
};

//  The errors joined by "; ", and the args with their fallbacks.
struct ArgsExpectation{
    std::string_view error = {};
    StoryActionArgs filled = {};
};

struct ArgsCase{
    ArgsInput input;
    ArgsExpectation expectation;
};

const ArgsCase CASES[] = {
    {.input = {.name = "mart", .args = {{"map", std::string("M")}, {"budget", int64_t(1500)}, {"if_present", true}}},
     .expectation = {.filled = {{"map", std::string("M")}, {"budget", int64_t(1500)}, {"floor", int64_t(0)}, {"if_present", true}}}},
    {.input = {.name = "fallback does not override", .args = {{"map", std::string("M")}, {"budget", int64_t(1)}, {"floor", int64_t(2)}}},
     .expectation = {.filled = {{"map", std::string("M")}, {"budget", int64_t(1)}, {"floor", int64_t(2)}}}},
    {.input = {.name = "unknown key", .args = {{"map", std::string("M")}, {"budget", int64_t(1)}, {"budgte", int64_t(1)}}},
     .expectation = {.error = "unknown argument \"budgte\"", .filled = {{"map", std::string("M")}, {"budget", int64_t(1)}, {"budgte", int64_t(1)}, {"floor", int64_t(0)}}}},
    {.input = {.name = "wrong type", .args = {{"map", std::string("M")}, {"budget", std::string("lots")}}},
     .expectation = {.error = "argument \"budget\" has the wrong type", .filled = {{"map", std::string("M")}, {"budget", std::string("lots")}, {"floor", int64_t(0)}}}},
    {.input = {.name = "missing required", .args = {{"budget", int64_t(1)}}},
     .expectation = {.error = "missing argument \"map\" (the Mart's map)", .filled = {{"budget", int64_t(1)}, {"floor", int64_t(0)}}}},
};


//  "" when `test` reads as expected, else what differs.
std::string check_case(const ArgsCase& test){
    std::string error;
    for (const std::string& each : story_args_errors(mart_spec(), test.input.args)){
        error += (error.empty() ? "" : "; ") + each;
    }
    if (error != test.expectation.error){
        return std::format("expected error \"{}\", got \"{}\"", test.expectation.error, error);
    }
    if (with_fallbacks(mart_spec(), test.input.args) != test.expectation.filled){
        return "filled in other args";
    }
    const StoryActionArgs& args = test.expectation.filled;
    const int64_t* floor = value_if_at_it<int64_t>(args, args.find("floor"));
    if (floor == nullptr || *floor != story_arg<int64_t>(args, "floor")){
        return "reads floor otherwise";
    }
    return "";
}


#ifndef PA_STORY_DATA_STANDALONE

class StoryActionArgTest : public UnitTest{
public:
    explicit StoryActionArgTest(ArgsCase test)
        : UnitTest(std::format("Pokemon::StoryActionArg - {}", test.input.name))
        , m_case(test)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_case(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }

private:
    ArgsCase m_case;
};

#endif


} // namespace


#ifndef PA_STORY_DATA_STANDALONE

void add_tests_StoryActionArg(UnitTestDatabase& database){
    for (const ArgsCase& test : CASES){
        database.add<StoryActionArgTest>(test);
    }
}

#endif



} // namespace Pokemon
} // namespace PokemonAutomation


#ifdef PA_STORY_DATA_STANDALONE

int main(){
    int failed = 0;
    for (const auto& test : PokemonAutomation::Pokemon::CASES){
        const std::string error = PokemonAutomation::Pokemon::check_case(test);
        std::cout << test.input.name << ": " << (error.empty() ? "PASS" : error) << '\n';
        failed += !error.empty();
    }
    return failed != 0;
}

#endif
