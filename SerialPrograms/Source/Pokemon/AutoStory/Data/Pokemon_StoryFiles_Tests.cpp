/*  Story Files Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon_StoryFiles.h"
#include "Pokemon_StoryFiles_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  "a b": the strings, space-separated.
std::string joined(const std::vector<std::string>& texts){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : " ") + part;
    }
    return text;
}

std::vector<std::string> ids(std::span<StoryCheckpoint> rows){
    std::vector<std::string> result;
    for (const StoryCheckpoint& row : rows){
        result.push_back(row.id);
    }
    return result;
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


class StoryFilesTest : public UnitTest{
public:
    explicit StoryFilesTest(RowsCase test_case)
        : UnitTest(std::format("Pokemon::StoryFiles - rows: {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_rows(m_case);
    }

private:
    RowsCase m_case;
};

} // namespace


void add_tests_StoryFiles(UnitTestDatabase& database){
    for (const RowsCase& test : ROWS_CASES){
        database.add<StoryFilesTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
