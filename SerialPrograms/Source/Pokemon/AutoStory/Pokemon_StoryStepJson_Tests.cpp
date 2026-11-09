/*  Story Step JSON Tests
 *  From: https://github.com/PokemonAutomation/
 */
#include <iostream>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Pokemon_StoryStepJson.h"
#ifndef PA_STORY_DATA_STANDALONE
#include "Pokemon_AutoStory_Tests.h"
#endif
namespace PokemonAutomation{
namespace Pokemon{
namespace{
enum class Record{ POSE, STEP, CHECKPOINT };
struct WireInput{
    std::string_view name;
    Record record = Record::STEP;
    std::string_view json;
};
struct WireExpectation{
    std::string_view canonical = {};
    bool rejected = false;
};
struct WireCase{
    WireInput input;
    WireExpectation expectation;
};
const WireCase CASES[] = {
    {.input = {.name = "default fields", .json = R"({"id":"talk","kind":"talk"})"}, .expectation = {.canonical = R"({"id":"talk","kind":"talk"})"}},
    {.input = {.name = "all shared fields", .json = R"({"id":"all","kind":"game_action","why":"source reason","direction":"west","legs":[{"direction":"south","tiles":0}],"fight_wild":true,"yes":false,"may_lose":true,"battles":3,"items":[{"item":"potion","quantity":0}],"variant":"profile","skip_if":["flag"],"target":{"map":"A","certainty":"unknown","where":{"kind":"unknown"}},"declared_after":{"map":"B","certainty":"set","where":{"kind":"tiles","tiles":[[0,0],[1,0]],"facing":"east"}}})"}, .expectation = {.canonical = R"({"id":"all","kind":"game_action","why":"source reason","direction":"west","legs":[{"direction":"south","tiles":0}],"fight_wild":true,"yes":false,"may_lose":true,"battles":3,"items":[{"item":"potion","quantity":0}],"variant":"profile","skip_if":["flag"],"target":{"map":"A","certainty":"unknown","where":{"kind":"unknown"}},"declared_after":{"map":"B","certainty":"set","where":{"kind":"tiles","tiles":[[0,0],[1,0]],"facing":"east"}}})"}},
    {.input = {.name = "unknown facing stays absent", .record = Record::POSE, .json = R"({"map":"M","certainty":"tile","where":{"kind":"tiles","tiles":[[0,0]]}})"}, .expectation = {.canonical = R"({"map":"M","certainty":"tile","where":{"kind":"tiles","tiles":[[0,0]]}})"}},
    {.input = {.name = "empty position differs from no position", .record = Record::POSE, .json = R"({"map":"M","certainty":"unknown","where":{"kind":"tiles","tiles":[]}})"}, .expectation = {.canonical = R"({"map":"M","certainty":"unknown","where":{"kind":"tiles","tiles":[]}})"}},
    {.input = {.name = "ordered checkpoint", .record = Record::CHECKPOINT, .json = R"({"id":"00.05","title":"Parcel","events":[{"id":"first","kind":"route"},{"id":"second","kind":"scene"}]})"}, .expectation = {.canonical = R"({"id":"00.05","title":"Parcel","events":[{"id":"first","kind":"route"},{"id":"second","kind":"scene"}]})"}},
    {.input = {.name = "unknown field", .json = R"({"id":"a","kind":"talk","mystery":0})"}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown action", .json = R"({"id":"a","kind":"magic"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "negative count", .json = R"({"id":"a","kind":"train","battles":-1})"}, .expectation = {.rejected = true}},
    {.input = {.name = "boolean count", .json = R"({"id":"a","kind":"train","battles":false})"}, .expectation = {.rejected = true}},
    {.input = {.name = "wrong bool", .json = R"({"id":"a","kind":"answer","yes":0})"}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown position", .record = Record::POSE, .json = R"({"map":"M","certainty":"unknown","where":{"kind":"gps"}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "bad coordinate", .record = Record::POSE, .json = R"({"map":"M","certainty":"tile","where":{"kind":"tiles","tiles":[[0,true]]}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "duplicate events", .record = Record::CHECKPOINT, .json = R"({"id":"x","title":"X","events":[{"id":"a","kind":"talk"},{"id":"a","kind":"talk"}]})"}, .expectation = {.rejected = true}},
};

JsonValue round_trip(const WireInput& input){
    const JsonValue value = parse_json(std::string(input.json));
    switch (input.record){
    case Record::POSE: return story_pose_json(read_story_pose(value, input.name));
    case Record::STEP: return story_step_json(read_story_step(value, input.name));
    case Record::CHECKPOINT: return story_checkpoint_json(read_story_checkpoint(value, input.name));
    }
    throw ParseException("Unknown test record.");
}

std::string check_case(const WireCase& test){
    try{
        const JsonValue actual = round_trip(test.input);
        if (test.expectation.rejected){
            return "Expected rejection, read accepted record.";
        }
        const JsonValue expected = parse_json(std::string(test.expectation.canonical));
        if (!(actual == expected)){
            return "Expected " + expected.dump() + ", read " + actual.dump();
        }
    }catch (const ParseException& error){
        return test.expectation.rejected ? "" : "Unexpected parse failure: " + error.message();
    }
    return "";
}
#ifdef PA_STORY_DATA_STANDALONE
Record record_kind(const std::string& name){
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
        : UnitTest("Pokemon::StoryStepJson - " + std::string(test.input.name)), m_case(test){}
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
