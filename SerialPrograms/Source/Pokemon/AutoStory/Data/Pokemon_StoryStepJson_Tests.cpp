/*  Story Step JSON Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each case reads a record from JSON and writes it back: the canonical
 *  form, or a rejection. Built standalone (PA_STORY_DATA_STANDALONE), it
 *  also checks the shared case file the Python reader checks.
 *
 */

#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Pokemon_StoryStepJson.h"
#ifndef PA_STORY_DATA_STANDALONE
#include "Pokemon_StoryStepJson_Tests.h"
#endif

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  Which record a case reads.
enum class Record{
    POSE,
    STEP,
    CHECKPOINT,
};

//  A record to read, and which kind it is.
struct WireInput{
    std::string_view name;
    Record record = Record::STEP;
    std::string_view json;
};

//  What reading it gives: the canonical record, or a rejection.
struct WireExpectation{
    std::string_view canonical = {};
    bool rejected = false;
};

struct WireCase{
    WireInput input;
    WireExpectation expectation;
};

const WireCase CASES[] = {
    {.input = {.name = "no args", .json = R"({"id":"talk","kind":"talk"})"}, .expectation = {.canonical = R"({"id":"talk","kind":"talk"})"}},
    {.input = {.name = "all shared fields", .json = R"({"id":"all","kind":"game_action","why":"source reason","action":"profile","skip_if":["flag"],"declared_after":{"map":"B","tiles":[[0,0],[1,0]],"facing":"east"}})"}, .expectation = {.canonical = R"({"id":"all","kind":"game_action","why":"source reason","action":"profile","skip_if":["flag"],"declared_after":{"map":"B","tiles":[[0,0],[1,0]],"facing":"east"}})"}},
    {.input = {.name = "route fills its fallback", .json = R"({"id":"r","kind":"route","args":{"target":{"map":"A","tiles":[[1,2]]}}})"}, .expectation = {.canonical = R"({"id":"r","kind":"route","args":{"fight_wild":false,"target":{"map":"A","tiles":[[1,2]]}}})"}},
    {.input = {.name = "legs", .json = R"({"id":"l","kind":"legs","action":"legs","args":{"legs":[{"direction":"south","tiles":2}],"fight_wild":true}})"}, .expectation = {.canonical = R"({"id":"l","kind":"legs","action":"legs","args":{"fight_wild":true,"legs":[{"direction":"south","tiles":2}]}})"}},
    {.input = {.name = "direction", .json = R"({"id":"s","kind":"step","args":{"direction":"west"}})"}, .expectation = {.canonical = R"({"id":"s","kind":"step","args":{"direction":"west"}})"}},
    {.input = {.name = "items", .json = R"({"id":"s","kind":"shop","args":{"items":[{"item":"potion","quantity":5}]}})"}, .expectation = {.canonical = R"({"id":"s","kind":"shop","args":{"items":[{"item":"potion","quantity":5}]}})"}},
    {.input = {.name = "game args", .json = R"({"id":"buy","kind":"game_action","action":"mart","args":{"map":"CeruleanCity_Mart","budget":3000,"if_present":true}})"}, .expectation = {.canonical = R"({"id":"buy","kind":"game_action","action":"mart","args":{"budget":3000,"if_present":true,"map":"CeruleanCity_Mart"}})"}},
    {.input = {.name = "game words", .json = R"({"id":"buy","kind":"game_action","action":"mart","args":{"map":"M","budget":1,"skip":["a","b"]}})"}, .expectation = {.canonical = R"({"id":"buy","kind":"game_action","action":"mart","args":{"budget":1,"if_present":false,"map":"M","skip":["a","b"]}})"}},
    {.input = {.name = "game integer rows", .json = R"({"id":"w","kind":"game_action","action":"walk","args":{"legs":[[0,4500],[-90,0],[]]}})"}, .expectation = {.canonical = R"({"id":"w","kind":"game_action","action":"walk","args":{"legs":[[0,4500],[-90,0],[]]}})"}},
    {.input = {.name = "integer rows, not rows", .json = R"({"id":"w","kind":"game_action","action":"walk","args":{"legs":[0,4500]}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "integer rows, fraction", .json = R"({"id":"w","kind":"game_action","action":"walk","args":{"legs":[[0,4.5]]}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown arg", .json = R"({"id":"a","kind":"game_action","action":"mart","args":{"map":"M","budget":1,"budgte":1}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "missing required arg", .json = R"({"id":"a","kind":"game_action","action":"mart","args":{"map":"M"}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "route without target", .json = R"({"id":"a","kind":"route"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "args on a kind with none", .json = R"({"id":"a","kind":"talk","args":{"yes":true}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "game step without action", .json = R"({"id":"a","kind":"game_action"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown action name", .json = R"({"id":"a","kind":"game_action","action":"dance"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "fractional arg", .json = R"({"id":"a","kind":"game_action","action":"mart","args":{"map":"M","budget":1.5}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "args not an object", .json = R"({"id":"a","kind":"game_action","action":"mart","args":["map"]})"}, .expectation = {.rejected = true}},
    {.input = {.name = "old field", .json = R"({"id":"a","kind":"battle","may_lose":true})"}, .expectation = {.rejected = true}},
    {.input = {.name = "old variant", .json = R"({"id":"a","kind":"game_action","variant":"mart"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown field", .json = R"({"id":"a","kind":"talk","mystery":0})"}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown kind", .json = R"({"id":"a","kind":"magic"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "boolean count", .json = R"({"id":"a","kind":"train","args":{"battles":false}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "wrong bool", .json = R"({"id":"a","kind":"answer","args":{"yes":0}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown facing stays absent", .record = Record::POSE, .json = R"({"map":"M","tiles":[[0,0]]})"}, .expectation = {.canonical = R"({"map":"M","tiles":[[0,0]]})"}},
    {.input = {.name = "no tiles", .record = Record::POSE, .json = R"({"map":"M","tiles":[]})"}, .expectation = {.canonical = R"({"map":"M","tiles":[]})"}},
    {.input = {.name = "old pose", .record = Record::POSE, .json = R"({"map":"M","certainty":"unknown","where":{"kind":"unknown"}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "bad coordinate", .record = Record::POSE, .json = R"({"map":"M","tiles":[[0,true]]})"}, .expectation = {.rejected = true}},
    {.input = {.name = "ordered checkpoint", .record = Record::CHECKPOINT, .json = R"({"id":"00.05","title":"Parcel","events":[{"id":"first","kind":"talk"},{"id":"second","kind":"scene"}]})"}, .expectation = {.canonical = R"({"id":"00.05","title":"Parcel","events":[{"id":"first","kind":"talk"},{"id":"second","kind":"scene"}]})"}},
    {.input = {.name = "duplicate events", .record = Record::CHECKPOINT, .json = R"({"id":"x","title":"X","events":[{"id":"a","kind":"talk"},{"id":"a","kind":"talk"}]})"}, .expectation = {.rejected = true}},
};

//  The built-in actions and a game's: a Mart step (GAME "mart") and a
//  profile check that takes no args.
std::vector<StoryActionSpec> test_actions(){
    std::vector<StoryActionSpec> actions(builtin_story_actions().begin(), builtin_story_actions().end());
    actions.push_back({.kind = StoryStepKind::GAME, .name = "mart", .args = {
        {.key = "map", .type = StoryArgType::WORD, .required = true, .fallback = std::nullopt, .why = "the Mart's map"},
        {.key = "budget", .type = StoryArgType::INTEGER, .required = true, .fallback = std::nullopt, .why = "money to spend"},
        {.key = "if_present", .type = StoryArgType::BOOLEAN, .required = false, .fallback = false, .why = ""},
        {.key = "skip", .type = StoryArgType::WORDS, .required = false, .fallback = std::nullopt, .why = ""},
    }});
    actions.push_back({.kind = StoryStepKind::GAME, .name = "walk", .args = {
        {.key = "legs", .type = StoryArgType::INTEGER_ROWS, .required = true, .fallback = std::nullopt, .why = "rows of numbers"},
    }});
    actions.push_back({.kind = StoryStepKind::GAME, .name = "profile", .args = {}});
    return actions;
}


//  `input` read and written back.
JsonValue round_trip(const WireInput& input){
    static const std::vector<StoryActionSpec> actions = test_actions();
    const JsonValue value = parse_json(std::string(input.json));
    switch (input.record){
    case Record::POSE:
        return grid_pose_json(read_grid_pose(value, input.name));
    case Record::STEP:
        return story_step_json(read_story_step(value, input.name, actions));
    case Record::CHECKPOINT:
        return story_checkpoint_json(read_story_checkpoint(value, input.name, actions));
    }
    throw ParseException("Unknown test record.");
}

//  "" when `test` reads as expected, else what was expected and what was read.
std::string check_case(const WireCase& test){
    try{
        const JsonValue actual = round_trip(test.input);
        if (test.expectation.rejected){
            return "Expected rejection, read accepted record.";
        }
        const JsonValue expected = parse_json(std::string(test.expectation.canonical));
        if (!(actual == expected)){
            return std::format("Expected {}, read {}", expected.dump(), actual.dump());
        }
    }catch (const ParseException& error){
        return test.expectation.rejected ? "" : std::format("Unexpected parse failure: {}", error.message());
    }
    return "";
}


#ifdef PA_STORY_DATA_STANDALONE

//  A shared case file's record kind.
Record record_kind(std::string_view name){
    if (name == "pose"){
        return Record::POSE;
    }
    if (name == "step"){
        return Record::STEP;
    }
    if (name == "checkpoint"){
        return Record::CHECKPOINT;
    }
    throw ParseException("Unsupported shared case record kind.");
}

//  Checks every case of the shared file `filename`; nonzero if any failed.
int check_shared_file(const std::string& filename){
    const JsonValue document = load_json_file(filename);
    const JsonObject& root = document.to_object_throw(filename);
    if (root.get_string_throw("format") != "pokesim-story-wire-cases/1"){
        throw ParseException("Unsupported story wire case format.");
    }
    int failed = 0;
    for (const JsonValue& value : root.get_array_throw("cases")){
        const JsonObject& row = value.to_object_throw(filename);
        const JsonObject& input = row.get_object_throw("input");
        const JsonObject& expected = row.get_object_throw("expectation");
        const std::string source = input.get_value_throw("value").dump();
        const JsonValue* normalized = expected.get_value("canonical");
        const std::string canonical = normalized ? normalized->dump() : "";
        const JsonValue* rejected = expected.get_value("rejected");
        const WireCase test{
            .input = {.name = input.get_string_throw("name"), .record = record_kind(input.get_string_throw("record")), .json = source},
            .expectation = {.canonical = canonical, .rejected = rejected && rejected->to_boolean_throw()},
        };
        const std::string error = check_case(test);
        if (!error.empty()){
            std::cerr << test.input.name << ": " << error << '\n';
            failed++;
        }
    }
    std::cout << "Checked " << root.get_array_throw("cases").size() << " shared story records.\n";
    return failed != 0;
}

#endif


#ifndef PA_STORY_DATA_STANDALONE

class StoryJsonTest : public UnitTest{
public:
    explicit StoryJsonTest(WireCase test)
        : UnitTest(std::format("Pokemon::StoryStepJson - {}", test.input.name))
        , m_case(test)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_case(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }

private:
    WireCase m_case;
};

#endif


} // namespace


#ifndef PA_STORY_DATA_STANDALONE

void add_tests_StoryStepJson(UnitTestDatabase& database){
    for (const WireCase& test : CASES){
        database.add<StoryJsonTest>(test);
    }
}

#endif



} // namespace Pokemon
} // namespace PokemonAutomation


#ifdef PA_STORY_DATA_STANDALONE

int main(int argc, char** argv){
    if (argc == 2){
        return PokemonAutomation::Pokemon::check_shared_file(argv[1]);
    }
    int failed = 0;
    for (const auto& test : PokemonAutomation::Pokemon::CASES){
        const std::string error = PokemonAutomation::Pokemon::check_case(test);
        std::cout << test.input.name << ": " << (error.empty() ? "PASS" : error) << '\n';
        failed += !error.empty();
    }
    return failed != 0;
}

#endif
