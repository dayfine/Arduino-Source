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
//  execution the shared fields and the game's own "note". 00.00 is its new
//  game, which starts without a save.
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

//  "rows 01.00 01.01 | titles Row Row | autosave 01.00 | unverified  | chained 01.01 | always "
std::string summary(const StoryData<StoryCheckpoint>& data){
    std::vector<std::string> titles;
    for (const StoryCheckpoint& row : data.checkpoints){
        titles.push_back(row.title);
    }
    return std::format(
        "rows {} | titles {} | autosave {} | unverified {} | chained {} | always {}",
        joined(ids(data.checkpoints)), joined(titles), joined(data.autosaves), joined(data.unverified_autosaves),
        joined(data.reload_when_chained), joined(data.reload_always)
    );
}


struct LoadCase{
    std::string name;
    struct Input{
        std::vector<EmbeddedJsonFile> builtin;
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
const EmbeddedJsonFile ROW_2 = {"test-01.02.plan", file_text("01.02", R"({"save": "unverified"})")};

const LoadCase LOAD_CASES[] = {
    {
        .name = "built-in files in order, their shared execution fields read",
        .input = {.builtin = {ROW_0, ROW_1, ROW_2}},
        .expectation = {.summary = "rows 01.00 01.01 01.02 | titles Row Row Row | autosave 01.00 | unverified 01.02 | chained 01.01 | always "},
    },
    {
        .name = "reload_when_chained always",
        .input = {.builtin = {ROW_0, {"test-01.01.plan", file_text("01.01", R"({"reload_when_chained": "always"})")}}},
        .expectation = {.summary = "rows 01.00 01.01 | titles Row Row | autosave 01.00 | unverified  | chained 01.01 | always 01.01"},
    },
    {
        .name = "an unknown execution field fails",
        .input = {.builtin = {{"test-01.00.plan", file_text("01.00", R"({"dev_mode": true})")}}},
        .expectation = {.error = "unknown execution field dev_mode"},
    },
    {
        .name = "save autosave",
        .input = {.builtin = {ROW_0}},
        .expectation = {.summary = "rows 01.00 | titles Row | autosave 01.00 | unverified  | chained  | always "},
    },
    {
        .name = "save unverified",
        .input = {.builtin = {{"test-01.00.plan", file_text("01.00", R"({"save": "unverified"})")}}},
        .expectation = {.summary = "rows 01.00 | titles Row | autosave  | unverified 01.00 | chained  | always "},
    },
    {
        .name = "save left out",
        .input = {.builtin = {{"test-01.00.plan", file_text("01.00", "{}")}}},
        .expectation = {.summary = "rows 01.00 | titles Row | autosave  | unverified  | chained  | always "},
    },
    {
        .name = "save menu fails",
        .input = {.builtin = {{"test-01.00.plan", file_text("01.00", R"({"save": "menu"})")}}},
        .expectation = {.error = "save is \"autosave\", \"unverified\" or left out"},
    },
    {
        .name = "a save other than autosave or unverified fails",
        .input = {.builtin = {{"test-01.00.plan", file_text("01.00", R"({"save": "card"})")}}},
        .expectation = {.error = "save is \"autosave\", \"unverified\" or left out"},
    },
    {
        .name = "reload_when_chained false fails",
        .input = {.builtin = {ROW_0, {"test-01.01.plan", file_text("01.01", R"({"reload_when_chained": false})")}}},
        .expectation = {.error = "reload_when_chained is true, \"always\" or left out"},
    },
    {
        .name = "reload_when_chained after a row that does not autosave fails",
        .input = {.builtin = {{"test-01.00.plan", file_text("01.00", "{}")}, ROW_1}},
        .expectation = {.error = "test-01.01.plan: reload_when_chained after a checkpoint that does not end on the game's autosave"},
    },
    {
        .name = "reload_when_chained after an unverified autosave fails",
        .input = {.builtin = {{"test-01.00.plan", file_text("01.00", R"({"save": "unverified"})")}, ROW_1}},
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
};

UnitTestResult check_load(const LoadCase& test){
    std::string got;
    std::string error;
    try{
        got = summary(load_story_data<StoryCheckpoint>(test.input.builtin, TEST_READER));
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
        size_t first = 0;
        size_t last = 0;
    };
    Input input;
    struct Expectation{
        std::string rows = {};
    };
    Expectation expectation;
};

//  01.00, 01.01, 01.02.
const RowsCase ROWS_CASES[] = {
    {.name = "every row", .input = {.first = 0, .last = 2}, .expectation = {.rows = "01.00 01.01 01.02"}},
    {.name = "a run from one row to a later one", .input = {.first = 1, .last = 2}, .expectation = {.rows = "01.01 01.02"}},
    {.name = "no rows for a start after the end", .input = {.first = 2, .last = 1}, .expectation = {.rows = ""}},
    {.name = "no rows for an end past the table", .input = {.first = 0, .last = 3}, .expectation = {.rows = ""}},
};

UnitTestResult check_rows(const RowsCase& test){
    std::vector<StoryCheckpoint> rows;
    for (std::string_view id : {"01.00", "01.01", "01.02"}){
        StoryCheckpoint row;
        row.id = std::string(id);
        rows.push_back(std::move(row));
    }
    const std::string got = joined(ids(story_run_rows<StoryCheckpoint>(rows, test.input.first, test.input.last)));
    if (got != test.expectation.rows){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.rows, got);
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
}



} // namespace Pokemon
} // namespace PokemonAutomation
