/*  BDSP Authored Checkpoint Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <set>
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

class AuthoredStoryLoadTest : public UnitTest{
public:
    AuthoredStoryLoadTest() : UnitTest("PokemonBDSP::StoryData - All 22 Authored Checkpoints Load"){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const auto& table = story_checkpoints();
        if (table.size() != 22){
            return std::format("Expected 22 authored checkpoints, got {}", table.size());
        }

        const std::vector<std::string> expected_ids{
            "00.00", "00.01", "00.02", "00.03", "00.04", "00.04b", "00.04c", "00.04d",
            "00.05", "00.05b", "00.05c",
            "01.01", "01.02", "01.03", "01.04", "01.05", "01.06", "01.07", "01.08", "01.09", "01.10", "01.11"
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
};

struct Case{
    std::string_view name;
    void (*mutate)(JsonObject&) = nullptr;
    bool rejected = false;
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

const Case REJECTION_CASES[] = {
    {.name = "valid 00.01 plan", .rejected = false},
    {.name = "extra envelope field", .mutate = extra, .rejected = true},
    {.name = "wrong game", .mutate = wrong_game, .rejected = true},
    {.name = "unknown failure policy", .mutate = wrong_policy, .rejected = true},
    {.name = "missing save-card location for reload_and_retry", .mutate = missing_card, .rejected = true},
    {.name = "stop policy with start save", .mutate = stop_with_save, .rejected = true},
};

class EnvelopeValidationTest : public UnitTest{
public:
    explicit EnvelopeValidationTest(Case test)
        : UnitTest("PokemonBDSP::StoryData - Envelope: " + std::string(test.name))
        , m_test(test)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const auto& table = story_checkpoints();
        auto it = std::find_if(table.begin(), table.end(), [](const StoryCheckpoint& cp){
            return cp.id == "00.01";
        });
        if (it == table.end()){
            return "00.01 checkpoint not found";
        }
        JsonValue val = authored_checkpoint_json(*it);
        if (m_test.mutate != nullptr){
            m_test.mutate(val.to_object_throw());
        }
        try{
            StoryCheckpoint cp = read_authored_checkpoint(val, m_test.name);
            if (m_test.rejected){
                return "Expected rejection but read_authored_checkpoint succeeded.";
            }
            if (cp.id != "00.01"){
                return "Loaded checkpoint id mismatch.";
            }
            return true;
        }catch (const Exception&){
            if (m_test.rejected){
                return true;
            }
            return "Unexpected rejection.";
        }
    }

private:
    Case m_test;
};

} // namespace

void add_tests_AutoStory_StoryData(UnitTestDatabase& database){
    database.add<AuthoredStoryLoadTest>();
    for (const Case& test : REJECTION_CASES){
        database.add<EnvelopeValidationTest>(test);
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
