/*  AutoStory Failure Capture Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The pure parts of the failure capture: its folder names, its JSON and
 *  the step a status line names. Saving needs a console's stream.
 *
 */

#include <string>
#include <utility>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_AutoStoryFailureCapture.h"
#include "Pokemon_AutoStory_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

FailureRecord oaks_lab_failure(){
    return FailureRecord{
        "PokemonFRLG", "00.27", "walk_to_oak", "expect_failed: dialogue opened",
        {"[Event] 00.27/3 walk_to_oak: start", "[Event] 00.27/3 walk_to_oak: failed"},
    };
}

UnitTestResult test_folder_name(){
    FailureRecord record = oaks_lab_failure();
    std::string name = failure_folder_name("20261109-110102123456", record);
    if (name != "20261109-110102123456-00.27-walk_to_oak"){
        return "Unexpected folder name: " + name;
    }
    record.event = "";
    name = failure_folder_name("t", record);
    if (name != "t-00.27-checkpoint"){
        return "A failed check should be named \"checkpoint\": " + name;
    }
    record.checkpoint = "00/27";
    record.event = "talk: Oak?";
    name = failure_folder_name("t", record);
    if (name != "t-00_27-talk__Oak_"){
        return "Characters a folder can't hold should be \"_\": " + name;
    }
    return true;
}

UnitTestResult test_json(){
    const JsonObject json = failure_json(oaks_lab_failure());
    if (json.get_string_throw("game") != "PokemonFRLG"
        || json.get_string_throw("checkpoint") != "00.27"
        || json.get_string_throw("event") != "walk_to_oak"
        || json.get_string_throw("error") != "expect_failed: dialogue opened"
    ){
        return "Unexpected fields: " + JsonValue(json.clone()).dump();
    }
    const JsonArray& log = json.get_array_throw("log");
    if (log.size() != 2 || log[1].to_string_throw() != "[Event] 00.27/3 walk_to_oak: failed"){
        return "Expected the log lines, oldest first: " + JsonValue(json.clone()).dump();
    }
    return true;
}

UnitTestResult test_step_of_status(){
    StatusStep step = step_of_status("00.03/2 below_oaks_trigger: start");
    if (step.checkpoint != "00.03" || step.event != "below_oaks_trigger"){
        return "Expected 00.03 below_oaks_trigger, got \"" + step.checkpoint + "\" \"" + step.event + "\"";
    }
    for (std::string_view line : {"00: start, 3 checkpoints", "00.02: passed on attempt 1 (one-shot)", "", "a/b"}){
        step = step_of_status(line);
        if (!step.checkpoint.empty() || !step.event.empty()){
            return std::string("Expected no step in \"") + std::string(line) + "\"";
        }
    }
    return true;
}

struct FailureCaptureInput{
    std::string name;
    UnitTestResult (*test)();
};

struct FailureCaptureExpectation{
    std::string_view description;
};

struct FailureCaptureTestCase{
    FailureCaptureInput input;
    FailureCaptureExpectation expectation;
};

class Test_AutoStoryFailureCapture : public UnitTest{
public:
    explicit Test_AutoStoryFailureCapture(FailureCaptureTestCase scenario)
        : UnitTest("Pokemon::AutoStoryFailureCapture - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_scenario.input.test();
    }

private:
    FailureCaptureTestCase m_scenario;
};

const FailureCaptureTestCase FAILURE_CAPTURE_CASES[] = {
    {
        .input = {.name = "folder name", .test = test_folder_name},
        .expectation = {.description = "names the folder by time, checkpoint and event, safe for a path"},
    },
    {
        .input = {.name = "json", .test = test_json},
        .expectation = {.description = "writes the record's fields and its log lines in order"},
    },
    {
        .input = {.name = "step of a status line", .test = test_step_of_status},
        .expectation = {.description = "reads checkpoint and event from event lines only"},
    },
};

} // namespace


void add_tests_AutoStoryFailureCapture(UnitTestDatabase& database){
    for (const auto& scenario : FAILURE_CAPTURE_CASES){
        database.add<Test_AutoStoryFailureCapture>(scenario);
    }
}


} // namespace Pokemon
} // namespace PokemonAutomation
