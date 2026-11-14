/*  AutoStory Table Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's checkpoint files read through the shared loader
 *  (load_frlg_story()): the authored rows in the story's order with their
 *  execution fields, and dev files in place of built-in ones.
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

namespace{


//  The ids of the authored rows in the order the story plays them, as
//  before the shared loader read them.
const std::vector<std::string_view> PINNED_ORDER = {
    "01.01", "01.02", "01.03", "01.04", "01.05", "01.06", "02.01", "02.02", "02.03", "03.01",
    "03.02", "03.03", "04.01", "04.02", "04.03", "04.04", "04.05", "04.06", "05.01", "05.02",
    "05.03", "05.04", "05.05", "05.06", "05.07", "05.08", "05.09", "05.10", "06.01", "06.02",
    "06.03", "06.04", "07.01", "07.02", "08.01", "08.02", "08.03", "09.01", "09.02", "09.03",
    "09.04", "10.01", "10.02", "10.03", "10.04", "10.05", "10.06", "11.01", "11.02", "11.03",
    "12.01", "12.02", "12.02b", "12.03",
    "13.01", "13.02", "13.03", "13.04", "14.01", "14.02", "14.02b",
    "14.03", "14.04", "14.04b", "15.01", "16.01", "16.02", "16.03", "16.03b", "16.04", "16.05", "16.06",
    "18.01", "18.02",
    "17.02", "17.03", "17.04", "17.04b", "19.01a", "19.01", "19.02", "19.03", "19.04", "19.04b", "19.04c", "19.04d", "19.05",
    "20.01a", "20.01", "20.02", "20.03", "20.04", "20.05",
};

//  The marks of every authored row that has one, as before the shared
//  loader: the RNG handoffs, their gifts and the live starts.
const std::string PINNED_MARKS =
    "01.01[sid_helper] 01.02[live] 01.05[starter_rng] 04.02[gift_rng] 04.03[live] "
    "09.03[gift_rng eevee] 09.04[live] 16.03b[gift_rng lapras] 16.04[live]";


//  A dev file in place of a built-in checkpoint file.
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
    root.get_object_throw("checkpoint")["id"] = "02.99";
}

void end_on_autosave(JsonObject& root){
    root.get_object_throw("execution")["save"] = "autosave";
}

void reload_when_chained(JsonObject& root){
    root.get_object_throw("execution")["reload_when_chained"] = true;
}

void unknown_execution_field(JsonObject& root){
    root.get_object_throw("execution")["frob"] = true;
}

JsonValue builtin_document(std::string_view name){
    for (const auto& [file, text] : autostory_checkpoint_json()){
        if (file == name){
            return parse_json(text);
        }
    }
    throw ParseException(std::format("No built-in checkpoint file {}", name));
}

std::vector<Pokemon::StoryDevFile> dev_files_of(const DevFileInput& input){
    std::vector<Pokemon::StoryDevFile> files;
    for (const DevFile& file : input.files){
        JsonValue document = builtin_document(file.builtin);
        if (file.mutate != nullptr){
            file.mutate(document.to_object_throw());
        }
        files.push_back({.name = file.file, .path = std::format("AutoStoryDev/frlg/{}", file.file), .text = document.dump()});
    }
    return files;
}

std::string_view handoff_name(StoryHandoff handoff){
    switch (handoff){
    case StoryHandoff::NONE:
        return "";
    case StoryHandoff::SID_HELPER:
        return "sid_helper";
    case StoryHandoff::STARTER_RNG:
        return "starter_rng";
    case StoryHandoff::GIFT_RNG:
        return "gift_rng";
    }
    return "";
}

//  The marks of row `row` of `story`: FRLG's handoff, gift and live start,
//  and the shared execution fields; empty if it has none.
std::string row_marks(const Pokemon::StoryData<StoryCheckpoint>& story, const StoryCheckpoint& row){
    std::vector<std::string> marks;
    if (row.handoff != StoryHandoff::NONE){
        marks.emplace_back(handoff_name(row.handoff));
    }
    if (row.gift == PokemonFRLG_RngTarget::eevee){
        marks.emplace_back("eevee");
    }
    if (row.gift == PokemonFRLG_RngTarget::lapras){
        marks.emplace_back("lapras");
    }
    if (row.live_start){
        marks.emplace_back("live");
    }
    if (story.autosaves.contains(row.id)){
        marks.emplace_back("autosave");
    }
    if (story.reload_when_chained.contains(row.id)){
        marks.emplace_back("reload");
    }
    if (story.dev_only.contains(row.id)){
        marks.emplace_back("dev_only");
    }
    std::string text;
    for (const std::string& mark : marks){
        text += text.empty() ? mark : " " + mark;
    }
    return text;
}

//  The loaded story in one line: whether its rows are PINNED_ORDER, 01.01's
//  title, the marks of each row that has some, how many rows a user sees
//  and the dev files used.
std::string story_summary(const Pokemon::StoryData<StoryCheckpoint>& story){
    bool in_order = story.checkpoints.size() == PINNED_ORDER.size();
    for (size_t c = 0; in_order && c < PINNED_ORDER.size(); c++){
        in_order = story.checkpoints[c].id == PINNED_ORDER[c];
    }
    std::string marks;
    for (const StoryCheckpoint& row : story.checkpoints){
        const std::string mark = row_marks(story, row);
        if (!mark.empty()){
            marks += std::format("{}{}[{}]", marks.empty() ? "" : " ", row.id, mark);
        }
    }
    std::string used;
    for (const auto& [name, path] : story.dev_files){
        used += std::format(" {}", name);
    }
    const size_t user_rows = Pokemon::story_rows_for_mode(story, Pokemon::StoryRowsShown::USER).size();
    return std::format(
        "{} | {} | {} | {} user rows | dev{}",
        in_order ? "story order" : "not story order", story.checkpoints.front().title, marks, user_rows, used
    );
}

std::string expected_summary(std::string_view title, std::string_view marks, size_t user_rows, std::string_view used){
    return std::format("story order | {} | {} | {} user rows | dev{}", title, marks, user_rows, used);
}

UnitTestResult check_dev_file_case(const DevFileCase& test_case){
    std::string summary;
    try{
        summary = story_summary(load_frlg_story(dev_files_of(test_case.input)));
    }catch (const ParseException& error){
        if (test_case.expectation.rejected){
            return true;
        }
        return std::format("Unexpected rejection: {}", error.message());
    }
    if (test_case.expectation.rejected){
        return "Expected rejection but load_frlg_story() succeeded.";
    }
    if (summary != test_case.expectation.summary){
        return std::format("Got \"{}\", expected \"{}\".", summary, test_case.expectation.summary);
    }
    return true;
}

class Test_StoryFiles : public UnitTest{
public:
    explicit Test_StoryFiles(DevFileCase test_case)
        : UnitTest(std::format("PokemonFRLG::StoryFiles - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_dev_file_case(m_case);
    }

private:
    DevFileCase m_case;
};

const std::string TITLE = "new game to the bedroom";
const size_t ROWS = PINNED_ORDER.size();

const DevFileCase DEV_FILE_CASES[] = {
    {
        .name = "No dev file: the authored rows in story order, marked as before",
        .input = {},
        .expectation = {.summary = expected_summary(TITLE, PINNED_MARKS, ROWS, "")},
    },
    {
        .name = "Dev file: replaces the built-in file of its name",
        .input = {.files = {{.file = "frlg-01.01.plan.json", .builtin = "frlg-01.01.plan", .mutate = retitle}}},
        .expectation = {.summary = expected_summary("Retimed", PINNED_MARKS, ROWS, " frlg-01.01.plan.json")},
    },
    {
        .name = "Dev file: a row numbered after the next stays in story order",
        .input = {.files = {{.file = "frlg-18.01.plan.json", .builtin = "frlg-18.01.plan"}}},
        .expectation = {.summary = expected_summary(TITLE, PINNED_MARKS, ROWS, " frlg-18.01.plan.json")},
    },
    {
        .name = "Dev file: a dev_only row is not listed for a user",
        .input = {.files = {{.file = "frlg-02.02.plan.json", .builtin = "frlg-02.02.plan", .mutate = mark_dev_only}}},
        .expectation = {.summary = expected_summary(
            TITLE, "01.01[sid_helper] 01.02[live] 01.05[starter_rng] 02.02[dev_only] 04.02[gift_rng] 04.03[live] "
            "09.03[gift_rng eevee] 09.04[live] 16.03b[gift_rng lapras] 16.04[live]",
            ROWS - 1, " frlg-02.02.plan.json"
        )},
    },
    {
        .name = "Dev file: 02.02 reloads after 02.01's autosave",
        .input = {.files = {
            {.file = "frlg-02.01.plan.json", .builtin = "frlg-02.01.plan", .mutate = end_on_autosave},
            {.file = "frlg-02.02.plan.json", .builtin = "frlg-02.02.plan", .mutate = reload_when_chained},
        }},
        .expectation = {.summary = expected_summary(
            TITLE, "01.01[sid_helper] 01.02[live] 01.05[starter_rng] 02.01[autosave] 02.02[reload] 04.02[gift_rng] 04.03[live] "
            "09.03[gift_rng eevee] 09.04[live] 16.03b[gift_rng lapras] 16.04[live]",
            ROWS, " frlg-02.01.plan.json frlg-02.02.plan.json"
        )},
    },
    {
        .name = "Dev file: a reload after a row that does not autosave, refused",
        .input = {.files = {{.file = "frlg-02.02.plan.json", .builtin = "frlg-02.02.plan", .mutate = reload_when_chained}}},
        .expectation = {.rejected = true},
    },
    {
        .name = "Dev file: a reload in the new game's row, refused",
        .input = {.files = {{.file = "frlg-01.01.plan.json", .builtin = "frlg-01.01.plan", .mutate = reload_when_chained}}},
        .expectation = {.rejected = true},
    },
    {
        .name = "Dev file: an execution field FRLG does not read, refused",
        .input = {.files = {{.file = "frlg-02.02.plan.json", .builtin = "frlg-02.02.plan", .mutate = unknown_execution_field}}},
        .expectation = {.rejected = true},
    },
    {
        .name = "Dev file: named like no built-in file, refused",
        .input = {.files = {{.file = "frlg-99.99.plan.json", .builtin = "frlg-01.01.plan"}}},
        .expectation = {.rejected = true},
    },
    {
        .name = "Dev file: changes the checkpoint id, refused",
        .input = {.files = {{.file = "frlg-02.02.plan.json", .builtin = "frlg-02.02.plan", .mutate = other_id}}},
        .expectation = {.rejected = true},
    },
};

} // namespace

void add_tests_AutoStoryStoryFiles(UnitTestDatabase& database){
    for (const auto& item : DEV_FILE_CASES){
        database.add<Test_StoryFiles>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
