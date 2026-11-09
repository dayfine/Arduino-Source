/*  Shared Parcel Scenario Native Tests
 *  From: https://github.com/PokemonAutomation/
 */
#include <algorithm>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_StoryReplay_IO.h"
#include "PokemonFRLG_StoryReplay.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace{
const JsonValue& named_case(const JsonValue& cases, std::string_view name){
    for (const JsonValue& value : cases.to_object_throw().get_array_throw("cases")){
        if (value.to_object_throw().get_string_throw("id") == name){
            return value;
        }
    }
    throw ParseException("native replay: unknown case");
}
std::string compare_expected(const JsonValue& test, const JsonValue& actual){
    const JsonObject& row = test.to_object_throw();
    const JsonObject& report = actual.to_object_throw();
    const bool actions = row.get_object_throw("input").get_boolean_default("check_actions");
    const JsonObject& expected = row.get_object_throw("expectation");
    if (actions != (expected.get_value("actions") != nullptr)){
        return "Expected actions only when check_actions is true.";
    }
    for (const auto& [key, value] : expected){
        const JsonValue* observed = report.get_value(key);
        if (!observed || value != *observed){
            return std::format("Expected {} {}, got {}", key, value.dump(-1), observed ? observed->dump(-1) : "missing");
        }
    }
    if (report.get_string_throw("status") == "model_complete"){
        for (const auto& [name, control] : report.get_object_throw("native_timing_controls")){
            if (control.to_object_throw().get_string_throw("status") != "complete"){
                return "Expected successful timed native control for " + name + ": " + control.dump(-1);
            }
        }
    }
    return "";
}
enum class Mutation{
    NONE, CAPTURE_HASH, MAP_HASH, CAPABILITY, SAVE_POLICY, CANDIDATE_DISTANCE,
    DROP_ACTION, EXTRA_ACTION, REVERSE_ACTIONS, ACTION_DISTANCE, ACTION_DIRECTION,
    //  A dialog before the walk to the Mart and before the clerk's scene
    //  (between_steps): the walk's cleared first, the scene's its own.
    DIALOGS_BETWEEN_STEPS,
};
enum class ExpectedResult{ MATCH, REJECTED, MISMATCH };
struct ReplayInput{
    std::string_view name;
    Mutation mutation = Mutation::NONE;
};
struct ReplayExpectation{
    ExpectedResult result = ExpectedResult::MATCH;
    std::string_view diagnostic;
};
struct ReplayCase{
    ReplayInput input;
    ReplayExpectation expectation;
};
const ReplayCase CASES[] = {
    {.input = {.name = "parcel-all-events"}, .expectation = {}},
    {.input = {.name = "entry-not-overworld"}, .expectation = {}},
    {.input = {.name = "missing-scene-evidence"}, .expectation = {}},
    {.input = {.name = "missing-parcel-receipt"}, .expectation = {}},
    {.input = {.name = "wrong-scene-arrival"}, .expectation = {}},
    {.input = {.name = "unconfirmed-mart-transition"}, .expectation = {}},
    {.input = {.name = "interrupted-route1-grass"}, .expectation = {}},
    {.input = {.name = "parcel-all-events", .mutation = Mutation::CAPTURE_HASH}, .expectation = {.result = ExpectedResult::REJECTED, .diagnostic = "capture hash mismatch"}},
    {.input = {.name = "parcel-all-events", .mutation = Mutation::MAP_HASH}, .expectation = {.result = ExpectedResult::REJECTED, .diagnostic = "map hash mismatch"}},
    {.input = {.name = "parcel-all-events", .mutation = Mutation::CAPABILITY}, .expectation = {.result = ExpectedResult::REJECTED, .diagnostic = "capability"}},
    {.input = {.name = "parcel-all-events", .mutation = Mutation::SAVE_POLICY}, .expectation = {.result = ExpectedResult::REJECTED, .diagnostic = "save"}},
    {.input = {.name = "parcel-all-events", .mutation = Mutation::CANDIDATE_DISTANCE}, .expectation = {.result = ExpectedResult::REJECTED, .diagnostic = "native"}},
    {.input = {.name = "parcel-all-events", .mutation = Mutation::DROP_ACTION}, .expectation = {.result = ExpectedResult::MISMATCH, .diagnostic = "actions"}},
    {.input = {.name = "parcel-all-events", .mutation = Mutation::EXTRA_ACTION}, .expectation = {.result = ExpectedResult::MISMATCH, .diagnostic = "actions"}},
    {.input = {.name = "parcel-all-events", .mutation = Mutation::REVERSE_ACTIONS}, .expectation = {.result = ExpectedResult::MISMATCH, .diagnostic = "actions"}},
    {.input = {.name = "parcel-all-events", .mutation = Mutation::ACTION_DISTANCE}, .expectation = {.result = ExpectedResult::MISMATCH, .diagnostic = "actions"}},
    {.input = {.name = "parcel-all-events", .mutation = Mutation::ACTION_DIRECTION}, .expectation = {.result = ExpectedResult::MISMATCH, .diagnostic = "actions"}},
    {.input = {.name = "parcel-all-events", .mutation = Mutation::DIALOGS_BETWEEN_STEPS}, .expectation = {}},
};
void mutate_config(Mutation mutation, JsonObject& binding, JsonObject& candidate){
    switch (mutation){
    case Mutation::CAPTURE_HASH:
        binding.get_object_throw("capture")["sha256"] = std::string(64, '0');
        return;
    case Mutation::MAP_HASH:
        binding.get_object_throw("maps")["Route1"] = std::string(64, '0');
        return;
    case Mutation::CAPABILITY:
        binding.get_object_throw("capabilities").get_object_throw("parcel_from_clerk")["capability"] = "unknown/1";
        return;
    case Mutation::SAVE_POLICY:
        binding.get_object_throw("policy")["save"] = "after";
        return;
    case Mutation::CANDIDATE_DISTANCE:
        candidate.get_object_throw("routes").get_array_throw("below_mart_door")[0].to_object_throw()["tiles"] = 2;
        return;
    case Mutation::NONE:
    case Mutation::DIALOGS_BETWEEN_STEPS:
    case Mutation::DROP_ACTION:
    case Mutation::EXTRA_ACTION:
    case Mutation::REVERSE_ACTIONS:
    case Mutation::ACTION_DISTANCE:
    case Mutation::ACTION_DIRECTION:
        return;
    }
}
void mutate_actions(Mutation mutation, JsonValue& report){
    JsonArray& actions = report.to_object_throw().get_array_throw("actions");
    JsonArray replacement;
    switch (mutation){
    case Mutation::DROP_ACTION:
        for (size_t n = 0; n + 1 < actions.size(); n++){
            replacement.push_back(actions[n].clone());
        }
        actions = std::move(replacement);
        return;
    case Mutation::EXTRA_ACTION:
        actions.push_back(actions[actions.size() - 1].clone());
        return;
    case Mutation::REVERSE_ACTIONS:
        for (size_t n = actions.size(); n > 0; n--){
            replacement.push_back(actions[n - 1].clone());
        }
        actions = std::move(replacement);
        return;
    case Mutation::ACTION_DISTANCE:
        actions[0].to_object_throw()["tiles"] = 2;
        return;
    case Mutation::ACTION_DIRECTION:
        actions[0].to_object_throw()["direction"] = "east";
        return;
    case Mutation::NONE:
    case Mutation::DIALOGS_BETWEEN_STEPS:
    case Mutation::CAPTURE_HASH:
    case Mutation::MAP_HASH:
    case Mutation::CAPABILITY:
    case Mutation::SAVE_POLICY:
    case Mutation::CANDIDATE_DISTANCE:
        return;
    }
}
//  `test` (a case of cases.json, which is PokemonSimulation's, so it has
//  no interrupts between steps) with a dialog before its walk to the Mart
//  and before the clerk's scene: expected as before, with the walk's
//  dialog cleared as its first action.
JsonValue with_dialogs_between_steps(const JsonValue& test){
    JsonValue out = test.clone();
    JsonObject& row = out.to_object_throw();
    JsonObject between;
    for (const char* step : {"below_mart_door", "parcel_from_clerk"}){
        JsonObject shown;
        shown["kind"] = "dialog";
        between[step] = std::move(shown);
    }
    row.get_object_throw("input")["between_steps"] = std::move(between);
    JsonArray& expected = row.get_object_throw("expectation").get_array_throw("actions");
    JsonArray actions;
    JsonObject cleared;
    cleared["event"] = "below_mart_door";
    cleared["kind"] = "interrupt";
    cleared["interrupt"] = "dialog";
    actions.push_back(std::move(cleared));
    for (const JsonValue& action : expected){
        actions.push_back(action.clone());
    }
    expected = std::move(actions);
    return out;
}
JsonValue run_case(const ReplayInput& input, const JsonValue& test){
    JsonValue binding = parse_json(story_replay_fixture("binding"));
    JsonValue candidate = parse_json(story_replay_fixture("route-candidate"));
    mutate_config(input.mutation, binding.to_object_throw(), candidate.to_object_throw());
    //  Re-pin the mutated test input so capability/policy negatives reach their
    //  intended validation instead of all failing at an unrelated byte hash.
    const std::string raw = binding.dump(-1);
    candidate.to_object_throw()["binding_sha256"] = StoryReplayInternal::digest(raw);
    JsonValue report = replay_story_checkpoint(story_replay_fixture("capture"), raw,
        candidate.dump(-1), test.to_object_throw().get_value_throw("input"));
    mutate_actions(input.mutation, report);
    return report;
}
class ReplayTest : public UnitTest{
public:
    explicit ReplayTest(ReplayCase test)
        : UnitTest(std::format("PokemonFRLG::StoryReplay - {} mutation {}", test.input.name, (int)test.input.mutation)), m_test(test){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const JsonValue document = parse_json(story_replay_fixture("cases"));
        const JsonValue& named = named_case(document, m_test.input.name);
        const JsonValue scripted = m_test.input.mutation == Mutation::DIALOGS_BETWEEN_STEPS
            ? with_dialogs_between_steps(named) : named.clone();
        const JsonValue& test = scripted;
        try{
            const JsonValue report = run_case(m_test.input, test);
            const std::string difference = compare_expected(test, report);
            const bool match = difference.empty();
            if (m_test.expectation.result == ExpectedResult::MATCH && match){
                return true;
            }
            if (m_test.expectation.result == ExpectedResult::MISMATCH && !match
                && difference.find(m_test.expectation.diagnostic) != std::string::npos){
                return true;
            }
            return "Unexpected replay result: " + difference + "\n" + report.dump(-1);
        }catch (const Exception& error){
            if (m_test.expectation.result == ExpectedResult::REJECTED
                && error.to_str().find(m_test.expectation.diagnostic) != std::string::npos){
                return true;
            }
            return "Unexpected native replay exception: " + error.to_str();
        }
    }
private:
    ReplayCase m_test;
};
} // namespace
void add_tests_StoryReplay(UnitTestDatabase& database){
    for (const ReplayCase& test : CASES){
        database.add<ReplayTest>(test);
    }
}
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
