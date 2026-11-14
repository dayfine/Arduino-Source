/*  Story Files Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryFiles.h"
#include "Pokemon_StoryFiles_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  A checkpoint file of the test game: {"id", "title", "execution"}, its
//  execution the shared fields and the game's own "note". Its lint refuses
//  the title "unlinted"; 00.00 is its new game, which starts without a save.
std::string file_text(std::string_view id, std::string_view execution, std::string_view title = "Row"){
    return std::format(R"({{"id": "{}", "title": "{}", "execution": {}}})", id, title, execution);
}

StoryFileRow<StoryCheckpoint> read_test_file(std::string_view text, std::string_view source){
    const JsonValue value = parse_json(std::string(text));
    const JsonObject& object = value.to_object_throw(std::string(source));
    StoryFileRow<StoryCheckpoint> row;
    row.checkpoint.id = object.get_string_throw("id", std::string(source));
    row.checkpoint.title = object.get_string_throw("title", std::string(source));
    row.execution = read_story_row_execution(object.get_object_throw("execution", std::string(source)), source, {"note"});
    return row;
}

const StoryFileReader<StoryCheckpoint> TEST_READER{
    .read = read_test_file,
    .lint = [](const StoryCheckpoint& checkpoint){
        return checkpoint.title == "unlinted" ? std::vector<std::string>{"title is unlinted"} : std::vector<std::string>{};
    },
    .starts_without_save = [](const StoryCheckpoint& checkpoint){
        return checkpoint.id == "00.00";
    },
};

//  "a b": the strings, space-separated.
std::string joined(const std::vector<std::string>& texts){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : " ") + part;
    }
    return text;
}
std::string joined(const std::set<std::string, std::less<>>& texts){
    return joined(std::vector<std::string>(texts.begin(), texts.end()));
}

std::vector<std::string> ids(const std::vector<StoryCheckpoint>& rows){
    std::vector<std::string> result;
    for (const StoryCheckpoint& row : rows){
        result.push_back(row.id);
    }
    return result;
}

//  "rows 01.00 01.01 | autosave 01.00 | chained 01.01 | always  | dev_only  | dev x.json"
std::string summary(const StoryData<StoryCheckpoint>& data){
    std::vector<std::string> dev;
    for (const auto& [name, path] : data.dev_files){
        dev.push_back(std::format("{}@{}", name, path));
    }
    std::vector<std::string> titles;
    for (const StoryCheckpoint& row : data.checkpoints){
        titles.push_back(row.title);
    }
    return std::format(
        "rows {} | titles {} | autosave {} | chained {} | always {} | dev_only {} | dev {}",
        joined(ids(data.checkpoints)), joined(titles), joined(data.autosaves), joined(data.reload_when_chained),
        joined(data.reload_always), joined(data.dev_only), joined(dev)
    );
}


struct LoadCase{
    std::string name;
    struct Input{
        std::vector<EmbeddedJsonFile> builtin;
        std::vector<StoryDevFile> dev_files = {};
        //  Whether the game plays its rows in id order, not file-name order.
        bool ordered_by_id = false;
    };
    Input input;
    struct Expectation{
        //  summary() of the story read; none when it fails.
        std::optional<std::string> summary = std::nullopt;
        //  What the ParseException's message contains when it fails.
        std::string error = {};
    };
    Expectation expectation;
};

const EmbeddedJsonFile ROW_0 = {"test-01.00.plan", file_text("01.00", R"({"save": "autosave"})")};
const EmbeddedJsonFile ROW_1 = {"test-01.01.plan", file_text("01.01", R"({"reload_when_chained": true, "note": 1})")};
const EmbeddedJsonFile ROW_2 = {"test-01.02.plan", file_text("01.02", R"({"dev_only": true, "save": "menu"})")};

StoryDevFile dev_file(std::string_view name, std::string text){
    return StoryDevFile{.name = std::string(name), .path = std::format("/dev/{}", name), .text = std::move(text)};
}

//  Named to come after 01.01 by file name, before it by id.
const EmbeddedJsonFile ROW_0B_MENU = {"test-01.10.plan", file_text("01.00b", R"({"save": "menu"})")};

const LoadCase LOAD_CASES[] = {
    {
        .name = "built-in files in order, their shared execution fields read",
        .input = {.builtin = {ROW_0, ROW_1, ROW_2}},
        .expectation = {.summary = "rows 01.00 01.01 01.02 | titles Row Row Row | autosave 01.00 | chained 01.01 | always  | dev_only 01.02 | dev "},
    },
    {
        .name = "reload_when_chained always",
        .input = {.builtin = {ROW_0, {"test-01.01.plan", file_text("01.01", R"({"reload_when_chained": "always"})")}}},
        .expectation = {.summary = "rows 01.00 01.01 | titles Row Row | autosave 01.00 | chained 01.01 | always 01.01 | dev_only  | dev "},
    },
    {
        .name = "a dev file replaces the built-in file of its name and is named",
        .input = {
            .builtin = {ROW_0, ROW_1},
            .dev_files = {dev_file("test-01.01.plan.json", file_text("01.01", R"({"reload_when_chained": true})", "Retimed"))},
        },
        .expectation = {.summary = "rows 01.00 01.01 | titles Row Retimed | autosave 01.00 | chained 01.01 | always  | dev_only  | dev test-01.01.plan.json@/dev/test-01.01.plan.json"},
    },
    {
        .name = "a dev file that is not JSON fails naming its path",
        .input = {.builtin = {ROW_0}, .dev_files = {dev_file("test-01.00.plan.json", "{ not json")}},
        .expectation = {.error = "/dev/test-01.00.plan.json"},
    },
    {
        .name = "a dev file with no built-in file of its name fails",
        .input = {.builtin = {ROW_0}, .dev_files = {dev_file("test-09.99.plan.json", file_text("09.99", "{}"))}},
        .expectation = {.error = "/dev/test-09.99.plan.json: no built-in checkpoint file is named test-09.99.plan.json"},
    },
    {
        .name = "a dev file that changes its checkpoint's id fails",
        .input = {.builtin = {ROW_0}, .dev_files = {dev_file("test-01.00.plan.json", file_text("01.09", R"({"save": "autosave"})"))}},
        .expectation = {.error = "/dev/test-01.00.plan.json: checkpoint id 01.09 does not match the built-in 01.00"},
    },
    {
        .name = "a dev file the game's lint refuses fails",
        .input = {.builtin = {ROW_0}, .dev_files = {dev_file("test-01.00.plan.json", file_text("01.00", "{}", "unlinted"))}},
        .expectation = {.error = "/dev/test-01.00.plan.json: title is unlinted"},
    },
    {
        .name = "an unknown execution field fails",
        .input = {.builtin = {{"test-01.00.plan", file_text("01.00", R"({"dev_mode": true})")}}},
        .expectation = {.error = "unknown execution field dev_mode"},
    },
    {
        .name = "dev_only false fails",
        .input = {.builtin = {{"test-01.00.plan", file_text("01.00", R"({"dev_only": false})")}}},
        .expectation = {.error = "dev_only is true or left out"},
    },
    {
        .name = "a save other than autosave or menu fails",
        .input = {.builtin = {{"test-01.00.plan", file_text("01.00", R"({"save": "card"})")}}},
        .expectation = {.error = "save is \"autosave\", \"menu\" or left out"},
    },
    {
        .name = "reload_when_chained false fails",
        .input = {.builtin = {ROW_0, {"test-01.01.plan", file_text("01.01", R"({"reload_when_chained": false})")}}},
        .expectation = {.error = "reload_when_chained is true, \"always\" or left out"},
    },
    {
        .name = "reload_when_chained after a row that does not autosave fails",
        .input = {.builtin = {{"test-01.00.plan", file_text("01.00", R"({"save": "menu"})")}, ROW_1}},
        .expectation = {.error = "test-01.01.plan: reload_when_chained after a checkpoint that does not end on the game's autosave"},
    },
    {
        .name = "reload_when_chained on the first row fails",
        .input = {.builtin = {ROW_1}},
        .expectation = {.error = "does not end on the game's autosave"},
    },
    {
        .name = "reload_when_chained on the new game fails",
        .input = {.builtin = {ROW_0, {"test-00.00.plan", file_text("00.00", R"({"reload_when_chained": true})")}}},
        .expectation = {.error = "test-00.00.plan: reload_when_chained on a checkpoint with no save to reload"},
    },
    {
        .name = "the game's order, its reload after the autosave row it follows there",
        .input = {.builtin = {ROW_1, ROW_0}, .ordered_by_id = true},
        .expectation = {.summary = "rows 01.00 01.01 | titles Row Row | autosave 01.00 | chained 01.01 | always  | dev_only  | dev "},
    },
    {
        .name = "the game's order, its reload after a row that does not autosave there fails",
        .input = {.builtin = {ROW_0, ROW_1, ROW_0B_MENU}, .ordered_by_id = true},
        .expectation = {.error = "test-01.01.plan: reload_when_chained after a checkpoint that does not end on the game's autosave"},
    },
};

UnitTestResult check_load(const LoadCase& test){
    StoryFileReader<StoryCheckpoint> reader = TEST_READER;
    if (test.input.ordered_by_id){
        reader.plays_before = [](const StoryCheckpoint& a, const StoryCheckpoint& b){
            return a.id < b.id;
        };
    }
    std::string got;
    std::string error;
    try{
        got = summary(load_story_data<StoryCheckpoint>(test.input.builtin, test.input.dev_files, reader));
    }catch (const ParseException& e){
        error = e.message();
    }
    if (test.expectation.summary){
        if (!error.empty()){
            return std::format("expected \"{}\", failed: {}", *test.expectation.summary, error);
        }
        if (got != *test.expectation.summary){
            return std::format("expected \"{}\", got \"{}\"", *test.expectation.summary, got);
        }
        return true;
    }
    if (error.find(test.expectation.error) == std::string::npos){
        return std::format("expected a failure with \"{}\", got \"{}\"", test.expectation.error, error.empty() ? got : error);
    }
    return true;
}


struct RowsCase{
    std::string name;
    struct Input{
        StoryRowsShown shown = StoryRowsShown::USER;
        size_t first = 0;
        size_t last = 0;
    };
    Input input;
    struct Expectation{
        std::string rows = {};
    };
    Expectation expectation;
};

//  01.00, 01.01, 01.02 with 01.01 dev-only.
const RowsCase ROWS_CASES[] = {
    {.name = "Developer Mode lists every row", .input = {.shown = StoryRowsShown::DEVELOPER, .first = 0, .last = 2}, .expectation = {.rows = "01.00 01.01 01.02"}},
    {.name = "a user's run skips the dev-only rows", .input = {.shown = StoryRowsShown::USER, .first = 0, .last = 1}, .expectation = {.rows = "01.00 01.02"}},
    {.name = "a run from one row to a later one", .input = {.shown = StoryRowsShown::DEVELOPER, .first = 1, .last = 2}, .expectation = {.rows = "01.01 01.02"}},
    {.name = "no rows for a start after the end", .input = {.shown = StoryRowsShown::DEVELOPER, .first = 2, .last = 1}, .expectation = {.rows = ""}},
    {.name = "no rows for an end past the table", .input = {.shown = StoryRowsShown::USER, .first = 0, .last = 2}, .expectation = {.rows = ""}},
};

UnitTestResult check_rows(const RowsCase& test){
    StoryData<StoryCheckpoint> story;
    for (std::string_view id : {"01.00", "01.01", "01.02"}){
        StoryCheckpoint row;
        row.id = std::string(id);
        story.checkpoints.push_back(std::move(row));
    }
    story.dev_only.emplace("01.01");
    const std::vector<StoryCheckpoint> shown = story_rows_for_mode(story, test.input.shown);
    const std::string got = joined(ids(story_run_rows<StoryCheckpoint>(shown, test.input.first, test.input.last)));
    if (got != test.expectation.rows){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.rows, got);
    }
    return true;
}


struct ReloadCase{
    std::string name;
    struct Input{
        StoryRunPolicy::BoundaryReload boundary_reload = StoryRunPolicy::BoundaryReload::AT_PLAN_BOUNDARIES;
        std::string row = {};
    };
    Input input;
    struct Expectation{
        bool reloads = false;
    };
    Expectation expectation;
};

//  01.01 reloads when chained, 01.02 always.
const ReloadCase RELOAD_CASES[] = {
    {.name = "at plan boundaries, a when-chained row reloads", .input = {.boundary_reload = StoryRunPolicy::BoundaryReload::AT_PLAN_BOUNDARIES, .row = "01.01"}, .expectation = {.reloads = true}},
    {.name = "straight through, a when-chained row runs on", .input = {.boundary_reload = StoryRunPolicy::BoundaryReload::STRAIGHT_THROUGH, .row = "01.01"}, .expectation = {.reloads = false}},
    {.name = "straight through, an always row reloads", .input = {.boundary_reload = StoryRunPolicy::BoundaryReload::STRAIGHT_THROUGH, .row = "01.02"}, .expectation = {.reloads = true}},
    {.name = "a row with no reload runs on", .input = {.boundary_reload = StoryRunPolicy::BoundaryReload::AT_PLAN_BOUNDARIES, .row = "01.00"}, .expectation = {.reloads = false}},
};

UnitTestResult check_reload(const ReloadCase& test){
    StoryRows rows;
    rows.reload_when_chained = {"01.01", "01.02"};
    rows.reload_always = {"01.02"};
    const bool got = story_reloads_before(rows, test.input.boundary_reload, test.input.row);
    if (got != test.expectation.reloads){
        return std::format("expected reloads {}, got {}", test.expectation.reloads, got);
    }
    return true;
}


template <typename Case>
class StoryFilesTest : public UnitTest{
public:
    using Check = UnitTestResult (*)(const Case&);

    StoryFilesTest(std::string_view suite, Case test_case, Check check)
        : UnitTest(std::format("Pokemon::StoryFiles - {}: {}", suite, test_case.name))
        , m_case(std::move(test_case))
        , m_check(check)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_check(m_case);
    }

private:
    Case m_case;
    Check m_check;
};

} // namespace


void add_tests_StoryFiles(UnitTestDatabase& database){
    for (const LoadCase& test : LOAD_CASES){
        database.add<StoryFilesTest<LoadCase>>("load", test, check_load);
    }
    for (const RowsCase& test : ROWS_CASES){
        database.add<StoryFilesTest<RowsCase>>("rows", test, check_rows);
    }
    for (const ReloadCase& test : RELOAD_CASES){
        database.add<StoryFilesTest<ReloadCase>>("reload", test, check_reload);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
