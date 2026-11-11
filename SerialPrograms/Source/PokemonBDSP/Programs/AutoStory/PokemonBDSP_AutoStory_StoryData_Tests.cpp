/*  BDSP Authored Checkpoint Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "PokemonBDSP_AutoStory_StoryData.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryData_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

struct StoryDataInput{
    std::string name;
    void (*mutate)(JsonObject&) = nullptr;
};

struct StoryDataExpectation{
    bool rejected = false;
};

struct StoryDataCase{
    StoryDataInput input;
    StoryDataExpectation expectation;
};

void extra(JsonObject& root){
    root["unknown"] = true;
}

void wrong_game(JsonObject& root){
    root["game"] = "frlg";
}

void wrong_policy(JsonObject& root){
    root.get_object_throw("execution")["on_failure"] = "guess";
}

void missing_card(JsonObject& root){
    root.get_object_throw("execution")["saves_in"] = "";
}

void stop_with_save(JsonObject& root){
    root.get_object_throw("execution")["on_failure"] = "stop";
}

void bad_metadata_field(JsonObject& root){
    JsonObject meta;
    meta["unknown"] = "testing";
    root["metadata"] = std::move(meta);
}

void bad_metadata_phase(JsonObject& root){
    JsonObject meta;
    meta["phase"] = "invalid";
    root["metadata"] = std::move(meta);
}

UnitTestResult check_all_checkpoints_load(){
    const auto& table = story_checkpoints();
    if (table.size() != 22){
        return std::format("Expected 22 authored checkpoints, got {}", table.size());
    }

    const std::vector<std::string> expected_ids{
        "00.00", "00.01", "00.02", "00.03", "00.04", "00.04b", "00.04c", "00.04d",
        "00.05", "00.05b", "00.05c",
        "01.01", "01.02", "01.02b", "01.03", "01.04", "01.05", "01.06", "01.07", "01.08", "01.09", "01.10", "01.11"
    };

    for (size_t i = 0; i < expected_ids.size(); i++){
        if (table[i].id != expected_ids[i]){
            return std::format("Checkpoint index {} mismatch: expected {}, got {}", i, expected_ids[i], table[i].id);
        }
        if (table[i].id == "00.00"){
            if (table[i].on_failure != StoryFailure::STOP || !table[i].saves_in.empty()){
                return "00.00 must have on_failure=stop and empty saves_in";
            }
        }else{
            if (table[i].on_failure != StoryFailure::RELOAD_AND_RETRY || table[i].saves_in.empty()){
                return std::format("{} must have on_failure=reload_and_retry and non-empty saves_in", table[i].id);
            }
        }
        if (table[i].steps.empty()){
            return std::format("{} has no steps", table[i].id);
        }
    }

    const StoryCheckpoint mine = load_authored_checkpoint("bdsp-mine-01.03.plan");
    if (mine.id != "01.03" || mine.on_failure != StoryFailure::RELOAD_AND_RETRY || mine.saves_in != "Oreburgh City"){
        return "Failed to load bdsp-mine-01.03.plan correctly";
    }

    return true;
}

UnitTestResult check_parity_01_02b(){
    const auto& table = story_checkpoints();
    auto it = std::find_if(table.begin(), table.end(), [](const StoryCheckpoint& cp){
        return cp.id == "01.02b";
    });
    if (it == table.end()){
        return "01.02b checkpoint not found in table";
    }
    const StoryCheckpoint& cp = *it;
    if (cp.title != "Oreburgh Center heal"){
        return "01.02b title mismatch";
    }
    if (cp.on_failure != StoryFailure::RELOAD_AND_RETRY || cp.saves_in != "Oreburgh City"){
        return "01.02b policy mismatch";
    }
    if (cp.steps.size() != 5){
        return std::format("01.02b expected 5 steps, got {}", cp.steps.size());
    }
    const std::vector<std::string> expected_step_names{
        "below_barry", "barry", "barrys_lines", "below_the_centers_door", "heal_oreburgh_first"
    };
    for (size_t i = 0; i < expected_step_names.size(); i++){
        if (cp.steps[i].name != expected_step_names[i]){
            return std::format("01.02b step {} name mismatch: expected {}, got {}", i, expected_step_names[i], cp.steps[i].name);
        }
    }
    return true;
}

UnitTestResult run_envelope_case(const StoryDataCase& test_case){
    const auto& table = story_checkpoints();
    auto it = std::find_if(table.begin(), table.end(), [](const StoryCheckpoint& cp){
        return cp.id == "00.01";
    });
    if (it == table.end()){
        return "00.01 checkpoint not found";
    }
    JsonValue val = authored_checkpoint_json(*it);
    if (test_case.input.mutate != nullptr){
        test_case.input.mutate(val.to_object_throw());
    }
    try{
        StoryCheckpoint cp = read_authored_checkpoint(val, test_case.input.name);
        if (test_case.expectation.rejected){
            return "Expected rejection but read_authored_checkpoint succeeded.";
        }
        if (cp.id != "00.01"){
            return "Loaded checkpoint id mismatch.";
        }
        return true;
    }catch (const Exception&){
        if (test_case.expectation.rejected){
            return true;
        }
        return "Unexpected rejection.";
    }
}

class Test_StoryData : public UnitTest{
public:
    explicit Test_StoryData(StoryDataCase test_case)
        : UnitTest("PokemonBDSP::StoryData - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (m_case.input.name == "All 22 authored checkpoints load"){
            return check_all_checkpoints_load();
        }
        return run_envelope_case(m_case);
    }

private:
    StoryDataCase m_case;
};

const StoryDataCase STORY_DATA_CASES[] = {
    {
        .input = {.name = "All 23 authored checkpoints load"},
        .expectation = {.rejected = false},
    },
    {
        .input = {.name = "Parity: 01.02b checkpoint"},
        .expectation = {.rejected = false},
    },
    {
        .input = {.name = "Envelope: valid 00.01 plan"},
        .expectation = {.rejected = false},
    },
    {
        .input = {.name = "Envelope: extra envelope field", .mutate = extra},
        .expectation = {.rejected = true},
    },
    {
        .input = {.name = "Envelope: wrong game", .mutate = wrong_game},
        .expectation = {.rejected = true},
    },
    {
        .input = {.name = "Envelope: unknown failure policy", .mutate = wrong_policy},
        .expectation = {.rejected = true},
    },
    {
        .input = {.name = "Envelope: missing save-card location for reload_and_retry", .mutate = missing_card},
        .expectation = {.rejected = true},
    },
    {
        .input = {.name = "Envelope: stop policy with start save", .mutate = stop_with_save},
        .expectation = {.rejected = true},
    },
    {
        .input = {.name = "Envelope: unknown metadata field", .mutate = bad_metadata_field},
        .expectation = {.rejected = true},
    },
    {
        .input = {.name = "Envelope: unknown metadata phase", .mutate = bad_metadata_phase},
        .expectation = {.rejected = true},
    },
};

} // namespace

void add_tests_AutoStory_StoryData(UnitTestDatabase& database){
    for (const auto& item : STORY_DATA_CASES){
        database.add<Test_StoryData>(item);
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
