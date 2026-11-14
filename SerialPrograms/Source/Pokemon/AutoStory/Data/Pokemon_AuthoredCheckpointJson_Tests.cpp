/*  Shared Authored Checkpoint JSON Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_AuthoredCheckpointJson.h"
#include "Pokemon_StoryStepJson.h"
#ifndef PA_STORY_DATA_STANDALONE
#include "Pokemon_AuthoredCheckpointJson_Tests.h"
#endif

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  Documents intentionally contain no game headers or native bookkeeping.
const std::string FRLG = R"({"format":"pokesim-authored-checkpoint/1","game":"frlg","checkpoint":{"id":"00.05","title":"Parcel","events":[{"id":"first","kind":"talk"},{"id":"second","kind":"scene"}]},"execution":{"menu":"before_pokedex"}})";
const std::string BDSP = R"({"format":"pokesim-authored-checkpoint/1","game":"bdsp","checkpoint":{"id":"01.03","title":"Mine","events":[{"id":"roark","kind":"talk"}]},"execution":{"on_failure":"reload_and_retry","saves_in":"Oreburgh City"}})";

//  How a case fails, if it does.
enum class Failure{
    NONE,
    PARSE,
    MISSING_RESOURCE,
};

struct AuthoredInput{
    std::string name;
    std::string json;
    std::string game = "frlg";
    bool resource = false;
    std::string file = "plan";
};

struct AuthoredExpectation{
    std::string checkpoint = "00.05";
    std::string execution = R"({"menu":"before_pokedex"})";
    std::vector<std::string> events = {"first", "second"};
    Failure failure = Failure::NONE;
};

struct AuthoredCase{
    AuthoredInput input;
    AuthoredExpectation expectation;
};

const AuthoredCase CASES[] = {
    {.input = {.name = "FRLG envelope", .json = FRLG}, .expectation = {}},
    {.input = {.name = "BDSP embedded resource", .json = BDSP, .game = "bdsp", .resource = true}, .expectation = {
        .checkpoint = "01.03", .execution = R"({"on_failure":"reload_and_retry","saves_in":"Oreburgh City"})", .events = {"roark"}}},
    {.input = {.name = "FRLG embedded resource", .json = FRLG, .resource = true}, .expectation = {}},
    {.input = {.name = "wrong game", .json = BDSP}, .expectation = {.failure = Failure::PARSE}},
    {.input = {.name = "empty expected game", .json = FRLG, .game = ""}, .expectation = {.failure = Failure::PARSE}},
    {.input = {.name = "missing resource", .json = FRLG, .resource = true, .file = "absent"}, .expectation = {.failure = Failure::MISSING_RESOURCE}},
    {.input = {.name = "invalid embedded JSON", .json = "{", .resource = true}, .expectation = {.failure = Failure::PARSE}},
    {.input = {.name = "wrong format", .json = R"({"format":"future","game":"frlg","checkpoint":{},"execution":{}})"}, .expectation = {.failure = Failure::PARSE}},
    {.input = {.name = "missing execution", .json = R"({"format":"pokesim-authored-checkpoint/1","game":"frlg","checkpoint":{}})"}, .expectation = {.failure = Failure::PARSE}},
    {.input = {.name = "same count wrong field", .json = R"({"format":"pokesim-authored-checkpoint/1","game":"frlg","checkpoint":{},"extra":{}})"}, .expectation = {.failure = Failure::PARSE}},
    {.input = {.name = "extra envelope field", .json = R"({"format":"pokesim-authored-checkpoint/1","game":"frlg","checkpoint":{},"execution":{},"extra":0})"}, .expectation = {.failure = Failure::PARSE}},
    {.input = {.name = "execution must be object", .json = R"({"format":"pokesim-authored-checkpoint/1","game":"frlg","checkpoint":{"id":"x","title":"X","events":[]},"execution":false})"}, .expectation = {.failure = Failure::PARSE}},
    {.input = {.name = "invalid common action", .json = R"({"format":"pokesim-authored-checkpoint/1","game":"frlg","checkpoint":{"id":"x","title":"X","events":[{"id":"bad","kind":"magic"}]},"execution":{}})"}, .expectation = {.failure = Failure::PARSE}},
    {.input = {.name = "execution stays game owned", .json = R"({"format":"pokesim-authored-checkpoint/1","game":"future","checkpoint":{"id":"x","title":"X","events":[]},"execution":{"zero":0,"false":false,"null":null}})", .game = "future"}, .expectation = {
        .checkpoint = "x", .execution = R"({"zero":0,"false":false,"null":null})", .events = {}}},
};


//  What a test reads: the checkpoint, and the execution object its reader
//  kept.
struct Loaded{
    StoryCheckpoint checkpoint;
    JsonObject execution;
};

//  A reader that keeps the execution object in `kept`.
ExecutionReader keep_execution(JsonObject& kept){
    return [&kept](const JsonObject& execution, std::string_view){
        kept = execution.clone();
    };
}

//  "" when `actual` is what `test` expects and survives a write and a
//  read, else what was expected and what was read.
std::string check_loaded(const Loaded& actual, const AuthoredCase& test){
    if (test.expectation.failure != Failure::NONE){
        return "Expected rejection, read accepted authored checkpoint.";
    }
    if (actual.checkpoint.id != test.expectation.checkpoint){
        return std::format("Expected checkpoint {}, read {}", test.expectation.checkpoint, actual.checkpoint.id);
    }
    if (!(JsonValue(actual.execution.clone()) == parse_json(test.expectation.execution))){
        return std::format("Expected execution {}, read {}", test.expectation.execution, actual.execution.dump());
    }
    std::vector<std::string> events;
    for (const StoryStep& step : actual.checkpoint.steps){
        events.push_back(step.name);
    }
    if (events != test.expectation.events){
        return "Expected authored event IDs in their original order.";
    }
    const JsonValue encoded = authored_checkpoint_json(actual.checkpoint, test.input.game, actual.execution.clone());
    Loaded reread;
    reread.checkpoint = read_authored_checkpoint(encoded, test.input.game, test.input.name, keep_execution(reread.execution));
    if (!(story_checkpoint_json(actual.checkpoint) == story_checkpoint_json(reread.checkpoint)) || actual.execution != reread.execution){
        return "Expected checkpoint and execution to survive the shared writer/reader round trip.";
    }
    return "";
}

//  "" when `test` reads or fails as expected.
std::string check_case(const AuthoredCase& test){
    try{
        const std::vector<std::pair<std::string, std::string>> files = {{"plan", test.input.json}};
        Loaded actual;
        actual.checkpoint = test.input.resource
            ? load_authored_checkpoint(files, test.input.file, test.input.game, keep_execution(actual.execution))
            : read_authored_checkpoint(parse_json(test.input.json), test.input.game, test.input.name, keep_execution(actual.execution));
        return check_loaded(actual, test);
    }catch (const ParseException& error){
        return test.expectation.failure == Failure::PARSE ? "" : std::format("Unexpected parse failure: {}", error.message());
    }catch (const InternalProgramError& error){
        return test.expectation.failure == Failure::MISSING_RESOURCE ? "" : std::format("Unexpected resource failure: {}", error.message());
    }
}


#ifndef PA_STORY_DATA_STANDALONE

class AuthoredJsonTest : public UnitTest{
public:
    explicit AuthoredJsonTest(AuthoredCase test)
        : UnitTest(std::format("Pokemon::AuthoredCheckpointJson - {}", test.input.name))
        , m_case(std::move(test))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_case(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }

private:
    AuthoredCase m_case;
};

#endif


} // namespace


#ifndef PA_STORY_DATA_STANDALONE

void add_tests_AuthoredCheckpointJson(UnitTestDatabase& database){
    for (const AuthoredCase& test : CASES){
        database.add<AuthoredJsonTest>(test);
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
