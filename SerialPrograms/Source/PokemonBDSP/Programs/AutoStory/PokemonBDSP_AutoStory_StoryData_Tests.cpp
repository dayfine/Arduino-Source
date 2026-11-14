/*  BDSP Authored Checkpoint Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string_view>
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

//  The built-in story table, plus one checkpoint file kept out of it.
struct StoryTableInput{
    std::string_view extra_file;
};

struct StoryTableExpectation{
    //  Every built-in checkpoint id, in story order. 00.00 stops on failure
    //  with no save; every other row reloads its save and retries.
    std::vector<std::string_view> ids;
    std::string_view extra_file_id;
    std::string_view extra_file_saves_in;
};

struct StoryTableCase{
    std::string_view name;
    StoryTableInput input;
    StoryTableExpectation expectation;
};

UnitTestResult check_story_table_case(const StoryTableCase& test_case){
    const StoryTableExpectation& expected = test_case.expectation;
    const auto& table = story_checkpoints();
    if (table.size() != expected.ids.size()){
        return std::format("Expected {} authored checkpoints, got {}", expected.ids.size(), table.size());
    }
    for (size_t i = 0; i < expected.ids.size(); i++){
        if (table[i].id != expected.ids[i]){
            return std::format("Checkpoint index {} mismatch: expected {}, got {}", i, expected.ids[i], table[i].id);
        }
        const bool first = i == 0;
        const StoryFailure policy = first ? StoryFailure::STOP : StoryFailure::RELOAD_AND_RETRY;
        if (table[i].on_failure != policy || table[i].saves_in.empty() != first){
            return std::format("{}: wrong on_failure or saves_in \"{}\"", table[i].id, table[i].saves_in);
        }
        if (table[i].steps.empty()){
            return std::format("{} has no steps", table[i].id);
        }
    }

    const StoryCheckpoint extra = load_authored_checkpoint(test_case.input.extra_file);
    if (extra.id != expected.extra_file_id || extra.on_failure != StoryFailure::RELOAD_AND_RETRY || extra.saves_in != expected.extra_file_saves_in){
        return std::format("Failed to load {} correctly", test_case.input.extra_file);
    }
    return true;
}

class Test_StoryTable : public UnitTest{
public:
    explicit Test_StoryTable(StoryTableCase test_case)
        : UnitTest(std::format("PokemonBDSP::StoryData - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_story_table_case(m_case);
    }

private:
    StoryTableCase m_case;
};

const StoryTableCase STORY_TABLE_CASES[] = {
    {
        .name = "All 23 authored checkpoints load",
        .input = {.extra_file = "bdsp-mine-01.03.plan"},
        .expectation = {
            .ids = {
                "00.00", "00.01", "00.02", "00.03", "00.04", "00.04b", "00.04c", "00.04d",
                "00.05", "00.05b", "00.05c",
                "01.01", "01.02", "01.02b", "01.03", "01.04", "01.05", "01.06", "01.07", "01.08", "01.09", "01.10", "01.11",
            },
            .extra_file_id = "01.03",
            .extra_file_saves_in = "Oreburgh City",
        },
    },
};


//  One checkpoint of the built-in story table, as loaded from its file.
struct CheckpointInput{
    std::string_view id;
};

struct CheckpointExpectation{
    std::string_view title;
    StoryFailure on_failure = StoryFailure::RELOAD_AND_RETRY;
    std::string_view saves_in;
    std::vector<std::string_view> step_names;
};

struct CheckpointCase{
    std::string_view name;
    CheckpointInput input;
    CheckpointExpectation expectation;
};

UnitTestResult check_checkpoint_case(const CheckpointCase& test_case){
    const CheckpointInput& input = test_case.input;
    const CheckpointExpectation& expected = test_case.expectation;
    const auto& table = story_checkpoints();
    auto it = std::find_if(table.begin(), table.end(), [&](const StoryCheckpoint& cp){
        return cp.id == input.id;
    });
    if (it == table.end()){
        return std::format("{} checkpoint not found in table", input.id);
    }
    const StoryCheckpoint& cp = *it;
    if (cp.title != expected.title){
        return std::format("{} title: expected \"{}\", got \"{}\"", input.id, expected.title, cp.title);
    }
    if (cp.on_failure != expected.on_failure || cp.saves_in != expected.saves_in){
        return std::format("{} policy mismatch (saves_in \"{}\")", input.id, cp.saves_in);
    }
    if (cp.steps.size() != expected.step_names.size()){
        return std::format("{} expected {} steps, got {}", input.id, expected.step_names.size(), cp.steps.size());
    }
    for (size_t i = 0; i < expected.step_names.size(); i++){
        if (cp.steps[i].name != expected.step_names[i]){
            return std::format("{} step {} name mismatch: expected {}, got {}", input.id, i, expected.step_names[i], cp.steps[i].name);
        }
    }
    return true;
}

class Test_StoryCheckpoint : public UnitTest{
public:
    explicit Test_StoryCheckpoint(CheckpointCase test_case)
        : UnitTest(std::format("PokemonBDSP::StoryData - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_checkpoint_case(m_case);
    }

private:
    CheckpointCase m_case;
};

const CheckpointCase CHECKPOINT_CASES[] = {
    {
        .name = "Parity: 01.02b checkpoint",
        .input = {.id = "01.02b"},
        .expectation = {
            .title = "Oreburgh Center heal",
            .saves_in = "Oreburgh City",
            .step_names = {"below_barry", "barry", "barrys_lines", "below_the_centers_door", "heal_oreburgh_first"},
        },
    },
};


//  bdsp-00.01.plan with one change to its envelope (read_authored_checkpoint()).
struct EnvelopeInput{
    void (*mutate)(JsonObject&) = nullptr;
};

struct EnvelopeExpectation{
    bool rejected = false;
};

struct EnvelopeCase{
    std::string_view name;
    EnvelopeInput input;
    EnvelopeExpectation expectation;
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

UnitTestResult run_envelope_case(const EnvelopeCase& test_case){
    constexpr std::string_view PLAN_FILE = "bdsp-00.01.plan";
    JsonValue val = authored_checkpoint_document(PLAN_FILE);
    if (test_case.input.mutate != nullptr){
        test_case.input.mutate(val.to_object_throw());
    }
    try{
        StoryCheckpoint cp = read_authored_checkpoint(val, PLAN_FILE);
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

class Test_StoryEnvelope : public UnitTest{
public:
    explicit Test_StoryEnvelope(EnvelopeCase test_case)
        : UnitTest(std::format("PokemonBDSP::StoryData - Envelope: {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return run_envelope_case(m_case);
    }

private:
    EnvelopeCase m_case;
};

const EnvelopeCase ENVELOPE_CASES[] = {
    {
        .name = "valid 00.01 plan",
        .input = {},
        .expectation = {.rejected = false},
    },
    {
        .name = "extra envelope field",
        .input = {.mutate = extra},
        .expectation = {.rejected = true},
    },
    {
        .name = "wrong game",
        .input = {.mutate = wrong_game},
        .expectation = {.rejected = true},
    },
    {
        .name = "unknown failure policy",
        .input = {.mutate = wrong_policy},
        .expectation = {.rejected = true},
    },
    {
        .name = "missing save-card location for reload_and_retry",
        .input = {.mutate = missing_card},
        .expectation = {.rejected = true},
    },
    {
        .name = "stop policy with start save",
        .input = {.mutate = stop_with_save},
        .expectation = {.rejected = true},
    },
    {
        .name = "unknown metadata field",
        .input = {.mutate = bad_metadata_field},
        .expectation = {.rejected = true},
    },
    {
        .name = "unknown metadata phase",
        .input = {.mutate = bad_metadata_phase},
        .expectation = {.rejected = true},
    },
};


//  A dev file in place of a built-in checkpoint file (load_bdsp_story()).
struct DevFile{
    std::string file;               //  the dev file's name
    std::string_view builtin;       //  the built-in file it starts from
    void (*mutate)(JsonObject&) = nullptr;
};

struct DevFileInput{
    std::vector<DevFile> files;
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

void end_on_autosave(JsonObject& root){
    root.get_object_throw("execution")["save"] = "autosave";
}

void reload_when_chained(JsonObject& root){
    root.get_object_throw("execution")["reload_when_chained"] = true;
}

std::vector<Pokemon::StoryDevFile> dev_files_of(const DevFileInput& input){
    std::vector<Pokemon::StoryDevFile> files;
    for (const DevFile& file : input.files){
        JsonValue document = authored_checkpoint_document(file.builtin);
        if (file.mutate != nullptr){
            file.mutate(document.to_object_throw());
        }
        files.push_back({.name = file.file, .path = std::format("AutoStoryDev/bdsp/{}", file.file), .text = document.dump()});
    }
    return files;
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
        .input = {},
        .expectation = {.summary = "Bedroom to the Verity Lakefront | dev_only | 23 user rows | dev"},
    },
    {
        .name = "Dev file: replaces the built-in file of its name",
        .input = {.files = {{.file = "bdsp-00.01.plan.json", .builtin = "bdsp-00.01.plan", .mutate = retitle}}},
        .expectation = {.summary = "Retimed | dev_only | 23 user rows | dev bdsp-00.01.plan.json"},
    },
    {
        .name = "Dev file: an out-of-order file's row stays in id order",
        .input = {.files = {{.file = "bdsp-mine-01.03.plan.json", .builtin = "bdsp-mine-01.03.plan"}}},
        .expectation = {.summary = "Bedroom to the Verity Lakefront | dev_only | 23 user rows | dev bdsp-mine-01.03.plan.json"},
    },
    {
        .name = "Dev file: bdsp-mine-01.03 reloads after 01.02b, the row before it by id",
        .input = {.files = {
            {.file = "bdsp-01.02b.plan.json", .builtin = "bdsp-01.02b.plan", .mutate = end_on_autosave},
            {.file = "bdsp-mine-01.03.plan.json", .builtin = "bdsp-mine-01.03.plan", .mutate = reload_when_chained},
        }},
        .expectation = {.summary = "Bedroom to the Verity Lakefront | dev_only | 23 user rows | dev bdsp-01.02b.plan.json bdsp-mine-01.03.plan.json"},
    },
    {
        .name = "Dev file: a dev_only row is not listed for a user",
        .input = {.files = {{.file = "bdsp-00.01.plan.json", .builtin = "bdsp-00.01.plan", .mutate = mark_dev_only}}},
        .expectation = {.summary = "Bedroom to the Verity Lakefront | dev_only 00.01 | 22 user rows | dev bdsp-00.01.plan.json"},
    },
    {
        .name = "Dev file: named like no built-in file, refused",
        .input = {.files = {{.file = "bdsp-99.99.plan.json", .builtin = "bdsp-00.01.plan"}}},
        .expectation = {.rejected = true},
    },
    {
        .name = "Dev file: changes the checkpoint id, refused",
        .input = {.files = {{.file = "bdsp-00.01.plan.json", .builtin = "bdsp-00.01.plan", .mutate = other_id}}},
        .expectation = {.rejected = true},
    },
};

} // namespace

void add_tests_AutoStory_StoryData(UnitTestDatabase& database){
    for (const auto& item : STORY_TABLE_CASES){
        database.add<Test_StoryTable>(item);
    }
    for (const auto& item : CHECKPOINT_CASES){
        database.add<Test_StoryCheckpoint>(item);
    }
    for (const auto& item : ENVELOPE_CASES){
        database.add<Test_StoryEnvelope>(item);
    }
    for (const auto& item : DEV_FILE_CASES){
        database.add<Test_StoryDevFiles>(item);
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
