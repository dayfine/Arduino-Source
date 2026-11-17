/*  Shared Authored Checkpoint JSON Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_AuthoredCheckpointJson.h"
#include "Pokemon_AuthoredCheckpointJson_Tests.h"

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
};

struct AuthoredCase{
    std::string name;
    struct Input{
        std::string json;
        std::string game = "frlg";
    };

    struct Expectation{
        std::string checkpoint = "00.05";
        std::string execution = R"({"menu":"before_pokedex"})";
        std::vector<std::string> events = {"first", "second"};
        Failure failure = Failure::NONE;
    };

    Input input;
    Expectation expectation;
};

const AuthoredCase CASES[] = {
    {.name = "FRLG envelope", .input = {.json = FRLG}, .expectation = {}},
    {.name = "BDSP envelope", .input = {.json = BDSP, .game = "bdsp"}, .expectation = {
        .checkpoint = "01.03", .execution = R"({"on_failure":"reload_and_retry","saves_in":"Oreburgh City"})", .events = {"roark"}}},
    {.name = "wrong game", .input = {.json = BDSP}, .expectation = {.failure = Failure::PARSE}},
    {.name = "empty expected game", .input = {.json = FRLG, .game = ""}, .expectation = {.failure = Failure::PARSE}},
    {.name = "wrong format", .input = {.json = R"({"format":"future","game":"frlg","checkpoint":{},"execution":{}})"}, .expectation = {.failure = Failure::PARSE}},
    {.name = "missing execution", .input = {.json = R"({"format":"pokesim-authored-checkpoint/1","game":"frlg","checkpoint":{}})"}, .expectation = {.failure = Failure::PARSE}},
    {.name = "same count wrong field", .input = {.json = R"({"format":"pokesim-authored-checkpoint/1","game":"frlg","checkpoint":{},"extra":{}})"}, .expectation = {.failure = Failure::PARSE}},
    {.name = "extra envelope field", .input = {.json = R"({"format":"pokesim-authored-checkpoint/1","game":"frlg","checkpoint":{},"execution":{},"extra":0})"}, .expectation = {.failure = Failure::PARSE}},
    {.name = "execution must be object", .input = {.json = R"({"format":"pokesim-authored-checkpoint/1","game":"frlg","checkpoint":{"id":"x","title":"X","events":[]},"execution":false})"}, .expectation = {.failure = Failure::PARSE}},
    {.name = "invalid common action", .input = {.json = R"({"format":"pokesim-authored-checkpoint/1","game":"frlg","checkpoint":{"id":"x","title":"X","events":[{"id":"bad","kind":"magic"}]},"execution":{}})"}, .expectation = {.failure = Failure::PARSE}},
    {.name = "execution stays game owned", .input = {.json = R"({"format":"pokesim-authored-checkpoint/1","game":"future","checkpoint":{"id":"x","title":"X","events":[]},"execution":{"zero":0,"false":false,"null":null}})", .game = "future"}, .expectation = {
        .checkpoint = "x", .execution = R"({"zero":0,"false":false,"null":null})", .events = {}}},
};


//  The actions the documents name: none, so each step is its kind's default.
std::span<const StoryActionSpec> story_actions(){
    return {};
}

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

//  "" when `actual` is what `test` expects, else what was expected and
//  what was read.
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
    return "";
}

//  "" when `test` reads or fails as expected.
std::string check_case(const AuthoredCase& test){
    try{
        Loaded actual;
        actual.checkpoint = read_authored_checkpoint(
            parse_json(test.input.json), test.input.game, test.name, keep_execution(actual.execution), story_actions()
        );
        return check_loaded(actual, test);
    }catch (const ParseException& error){
        return test.expectation.failure == Failure::PARSE ? "" : std::format("Unexpected parse failure: {}", error.message());
    }
}


class AuthoredJsonTest : public UnitTest{
public:
    explicit AuthoredJsonTest(AuthoredCase test)
        : UnitTest(std::format("Pokemon::AuthoredCheckpointJson - {}", test.name))
        , m_case(std::move(test))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_case(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }

private:
    AuthoredCase m_case;
};


} // namespace


void add_tests_AuthoredCheckpointJson(UnitTestDatabase& database){
    for (const AuthoredCase& test : CASES){
        database.add<AuthoredJsonTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
