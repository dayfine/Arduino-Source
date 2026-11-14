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
#include "Pokemon/AutoStory/Data/Pokemon_StoryFiles.h"
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
    JsonValue val = authored_checkpoint_document("bdsp-00.01.plan");
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


//  A dev file in place of a built-in checkpoint file (load_bdsp_story()).
struct DevFileInput{
    std::string file;               //  the dev file's name; empty: no dev file
    std::string_view builtin;       //  the built-in file it starts from
    void (*mutate)(JsonObject&) = nullptr;
};

struct DevFileExpectation{
    bool rejected = false;
    //  story_summary() of the story loaded.
    std::string summary = {};
};

struct DevFileCase{
    std::string name;
    DevFileInput input;
    DevFileExpectation expectation;
};

void retitle(JsonObject& root){
    root.get_object_throw("checkpoint")["title"] = "Retimed";
}

void mark_dev_only(JsonObject& root){
    root.get_object_throw("execution")["dev_only"] = true;
}

void other_id(JsonObject& root){
    root.get_object_throw("checkpoint")["id"] = "00.99";
}

std::vector<Pokemon::StoryDevFile> dev_files_of(const DevFileInput& input){
    if (input.file.empty()){
        return {};
    }
    JsonValue document = authored_checkpoint_document(input.builtin);
    if (input.mutate != nullptr){
        input.mutate(document.to_object_throw());
    }
    return {{.name = input.file, .path = std::format("AutoStoryDev/bdsp/{}", input.file), .text = document.dump()}};
}

//  The loaded story in one line: 00.01's title, the dev_only rows, how
//  many rows a user sees and the dev files used; or "not in id order".
std::string story_summary(const Pokemon::StoryData<StoryCheckpoint>& story){
    const bool in_id_order = std::is_sorted(story.checkpoints.begin(), story.checkpoints.end(),
        [](const StoryCheckpoint& a, const StoryCheckpoint& b){ return a.id < b.id; });
    if (!in_id_order){
        return "not in id order";
    }
    std::string title;
    for (const StoryCheckpoint& row : story.checkpoints){
        if (row.id == "00.01"){
            title = row.title;
        }
    }
    std::string dev_only;
    for (const std::string& id : story.dev_only){
        dev_only += std::format(" {}", id);
    }
    std::string used;
    for (const auto& [name, path] : story.dev_files){
        used += std::format(" {}", name);
    }
    const size_t user_rows = Pokemon::story_rows_for_mode(story, Pokemon::StoryRowsShown::USER).size();
    return std::format("{} | dev_only{} | {} user rows | dev{}", title, dev_only, user_rows, used);
}

UnitTestResult check_dev_file_case(const DevFileCase& test_case){
    std::string summary;
    try{
        summary = story_summary(load_bdsp_story(dev_files_of(test_case.input)));
    }catch (const ParseException& error){
        if (test_case.expectation.rejected){
            return true;
        }
        return std::format("Unexpected rejection: {}", error.message());
    }
    if (test_case.expectation.rejected){
        return "Expected rejection but load_bdsp_story() succeeded.";
    }
    if (summary != test_case.expectation.summary){
        return std::format("Got \"{}\", expected \"{}\".", summary, test_case.expectation.summary);
    }
    return true;
}

class Test_StoryDevFiles : public UnitTest{
public:
    explicit Test_StoryDevFiles(DevFileCase test_case)
        : UnitTest(std::format("PokemonBDSP::StoryData - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_dev_file_case(m_case);
    }

private:
    DevFileCase m_case;
};

const DevFileCase DEV_FILE_CASES[] = {
    {
        .name = "Dev file: none, the built-in rows in id order",
        .input = {.file = "", .builtin = ""},
        .expectation = {.summary = "Bedroom to the Verity Lakefront | dev_only | 23 user rows | dev"},
    },
    {
        .name = "Dev file: replaces the built-in file of its name",
        .input = {.file = "bdsp-00.01.plan.json", .builtin = "bdsp-00.01.plan", .mutate = retitle},
        .expectation = {.summary = "Retimed | dev_only | 23 user rows | dev bdsp-00.01.plan.json"},
    },
    {
        .name = "Dev file: an out-of-order file's row stays in id order",
        .input = {.file = "bdsp-mine-01.03.plan.json", .builtin = "bdsp-mine-01.03.plan"},
        .expectation = {.summary = "Bedroom to the Verity Lakefront | dev_only | 23 user rows | dev bdsp-mine-01.03.plan.json"},
    },
    {
        .name = "Dev file: a dev_only row is not listed for a user",
        .input = {.file = "bdsp-00.01.plan.json", .builtin = "bdsp-00.01.plan", .mutate = mark_dev_only},
        .expectation = {.summary = "Bedroom to the Verity Lakefront | dev_only 00.01 | 22 user rows | dev bdsp-00.01.plan.json"},
    },
    {
        .name = "Dev file: named like no built-in file, refused",
        .input = {.file = "bdsp-99.99.plan.json", .builtin = "bdsp-00.01.plan"},
        .expectation = {.rejected = true},
    },
    {
        .name = "Dev file: changes the checkpoint id, refused",
        .input = {.file = "bdsp-00.01.plan.json", .builtin = "bdsp-00.01.plan", .mutate = other_id},
        .expectation = {.rejected = true},
    },
};

} // namespace

void add_tests_AutoStory_StoryData(UnitTestDatabase& database){
    for (const auto& item : STORY_DATA_CASES){
        database.add<Test_StoryData>(item);
    }
    for (const auto& item : DEV_FILE_CASES){
        database.add<Test_StoryDevFiles>(item);
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
