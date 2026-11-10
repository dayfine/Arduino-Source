/*  Story Args Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checking a step's args against an action's specs, and reading an old
 *  "name key=value" variant into a name and args. Built standalone
 *  (PA_STORY_DATA_STANDALONE), it runs without the application.
 *
 */

#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include "Pokemon_StoryVariant.h"
#include "Types/Pokemon_StoryArgs.h"
#ifndef PA_STORY_DATA_STANDALONE
#include "Pokemon_AutoStory_Tests.h"
#endif

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  The args a Mart step takes, as an action declares them.
constexpr StoryArgSpec MART_SPECS[] = {
    {.key = "map", .type = StoryArgType::TEXT, .required = true},
    {.key = "budget", .type = StoryArgType::NUMBER, .required = true},
    {.key = "floor", .type = StoryArgType::NUMBER},
    {.key = "if_present", .type = StoryArgType::FLAG},
};

constexpr std::string_view MART_POSITIONAL[] = {"map"};


//  An old variant, read with an action's positional keys and checked
//  against its specs.
struct ArgsInput{
    std::string_view name;
    std::string_view variant;
    std::span<const StoryArgSpec> specs = MART_SPECS;
    std::span<const std::string_view> positional = MART_POSITIONAL;
};

//  The action name and args it gives, and the first error: the split's,
//  else the specs'.
struct ArgsExpectation{
    std::string_view action;
    StoryArgs args = {};
    std::string_view error = {};
};

struct ArgsCase{
    ArgsInput input;
    ArgsExpectation expectation;
};

const ArgsCase CASES[] = {
    {.input = {.name = "mart", .variant = "mart CeruleanCity_Mart budget=1500 floor=1500 if_present"},
     .expectation = {.action = "mart", .args = {{"map", std::string("CeruleanCity_Mart")}, {"budget", int64_t(1500)}, {"floor", int64_t(1500)}, {"if_present", true}}}},
    {.input = {.name = "plain name", .variant = "run", .specs = {}, .positional = {}}, .expectation = {.action = "run"}},
    {.input = {.name = "words stay words", .variant = "teach tutor forget=tail-whip", .specs = {}, .positional = {}},
     .expectation = {.action = "teach", .args = {{"tutor", true}, {"forget", std::string("tail-whip")}}, .error = "unknown argument \"forget\""}},
    {.input = {.name = "unknown key", .variant = "mart M budget=1 budgte=1"},
     .expectation = {.action = "mart", .args = {{"map", std::string("M")}, {"budget", int64_t(1)}, {"budgte", int64_t(1)}}, .error = "unknown argument \"budgte\""}},
    {.input = {.name = "wrong type", .variant = "mart M budget=lots"},
     .expectation = {.action = "mart", .args = {{"map", std::string("M")}, {"budget", std::string("lots")}}, .error = "argument \"budget\" has the wrong type"}},
    {.input = {.name = "missing required", .variant = "mart M"},
     .expectation = {.action = "mart", .args = {{"map", std::string("M")}}, .error = "missing argument \"budget\""}},
    {.input = {.name = "repeated key", .variant = "mart M budget=1 budget=2"},
     .expectation = {.action = "mart", .args = {{"map", std::string("M")}, {"budget", int64_t(1)}}, .error = "\"budget=2\" repeats an argument"}},
};


//  "" when `test` reads as expected, else what differs.
std::string check_case(const ArgsCase& test){
    const LegacyVariant read = split_legacy_variant(test.input.variant, test.input.positional);
    const std::string error = read.error.empty() ? story_args_error(read.args, test.input.specs) : read.error;
    if (read.name != test.expectation.action || read.args != test.expectation.args){
        return "read \"" + read.name + "\" with other args";
    }
    if (error != test.expectation.error){
        return "expected error \"" + std::string(test.expectation.error) + "\", got \"" + error + "\"";
    }
    return "";
}


#ifndef PA_STORY_DATA_STANDALONE

class StoryArgsTest : public UnitTest{
public:
    explicit StoryArgsTest(ArgsCase test)
        : UnitTest("Pokemon::StoryArgs - " + std::string(test.input.name))
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

void add_tests_StoryArgs(UnitTestDatabase& database){
    for (const ArgsCase& test : CASES){
        database.add<StoryArgsTest>(test);
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
