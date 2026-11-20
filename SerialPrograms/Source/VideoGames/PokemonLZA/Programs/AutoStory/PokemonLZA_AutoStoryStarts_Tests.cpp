/*  PokemonLZA AutoStory - Checkpoint starts Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryStarts_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::StoryCheckpoint;

//  The index of row `id` in `rows`; none when there is no such row.
std::optional<size_t> index_of(const std::vector<StoryCheckpoint>& rows, std::string_view id){
    for (size_t index = 0; index < rows.size(); index++){
        if (rows[index].id == id){
            return index;
        }
    }
    return std::nullopt;
}

//  Where a run asked to start at `asked` plays from, by `read`, with the
//  checkpoints `rows` and the starts `starts`: a checkpoint id, or
//  "stop" for start_position_unknown.
std::string run_start_id(
    const std::vector<StoryCheckpoint>& rows, const std::vector<CheckpointStart>& starts,
    std::string_view asked, const StartSignature& read
){
    const std::optional<size_t> asked_index = index_of(rows, asked);
    if (!asked_index){
        return std::format("no checkpoint {}", asked);
    }
    const std::optional<size_t> from = run_start(rows, starts, *asked_index, read);
    return from ? rows[*from].id : std::string("stop");
}




//  Runs against a made-up table, for the rules no built-in plan shows.
struct TableCase{
    std::string name;
    struct Input{
        std::vector<CheckpointStart> starts;
        std::string asked;
        StartSignature read;
    };
    Input input;
    struct Expectation{
        std::string from;   //  A checkpoint id, or "stop".
    };
    Expectation expectation;
};

const StartSignature AT_COUNTER{.talk_prompt = true, .wild_zone = false};
const StartSignature IN_ZONE{.wild_zone = true};

const TableCase TABLE_CASES[] = {
    {
        .name = "asked has no signature, the screen matches no start: runs as asked",
        .input = {.starts = {{.checkpoint = "A", .signature = AT_COUNTER}}, .asked = "C", .read = IN_ZONE},
        .expectation = {.from = "C"},
    },
    {
        .name = "asked has no signature, the screen matches an earlier start: runs as asked",
        .input = {.starts = {{.checkpoint = "A", .signature = AT_COUNTER}}, .asked = "C", .read = AT_COUNTER},
        .expectation = {.from = "C"},
    },
    {
        .name = "asked has no signature, two earlier starts match: runs as asked",
        .input = {
            .starts = {{.checkpoint = "A", .signature = {.wild_zone = false}}, {.checkpoint = "B", .signature = AT_COUNTER}},
            .asked = "C", .read = AT_COUNTER,
        },
        .expectation = {.from = "C"},
    },
    {
        .name = "two earlier starts match the signed asked's screen: stop",
        .input = {
            .starts = {
                {.checkpoint = "A", .signature = {.wild_zone = false}},
                {.checkpoint = "B", .signature = AT_COUNTER},
                {.checkpoint = "C", .signature = IN_ZONE},
            },
            .asked = "C", .read = AT_COUNTER,
        },
        .expectation = {.from = "stop"},
    },
    {
        .name = "a later start matches, the asked one does not: stop",
        .input = {
            .starts = {{.checkpoint = "B", .signature = IN_ZONE}, {.checkpoint = "C", .signature = AT_COUNTER}},
            .asked = "B", .read = AT_COUNTER,
        },
        .expectation = {.from = "stop"},
    },
    {
        .name = "the asked start matches, an earlier one too: as asked",
        .input = {
            .starts = {{.checkpoint = "A", .signature = AT_COUNTER}, {.checkpoint = "B", .signature = {.talk_prompt = true}}},
            .asked = "B", .read = AT_COUNTER,
        },
        .expectation = {.from = "B"},
    },
    {
        //  03.09 and a later row may both start out of a wild zone with 5
        //  in the party: a chained run checks each against its own.
        .name = "two starts share a signature, the asked one is one of them: as asked",
        .input = {
            .starts = {{.checkpoint = "B", .signature = AT_COUNTER}, {.checkpoint = "C", .signature = AT_COUNTER}},
            .asked = "C", .read = AT_COUNTER,
        },
        .expectation = {.from = "C"},
    },
    {
        .name = "a reading sharing no field with the asked start: stop",
        .input = {.starts = {{.checkpoint = "B", .signature = {.registered = 1}}}, .asked = "B", .read = AT_COUNTER},
        .expectation = {.from = "stop"},
    },
};

//  Checkpoints A, B and C, in that order.
std::vector<StoryCheckpoint> table_rows(){
    std::vector<StoryCheckpoint> rows;
    for (std::string_view id : {"A", "B", "C"}){
        StoryCheckpoint row;
        row.id = std::string(id);
        row.title = "Test";
        rows.push_back(std::move(row));
    }
    return rows;
}

UnitTestResult check_table(const TableCase& test){
    const std::string from = run_start_id(table_rows(), test.input.starts, test.input.asked, test.input.read);
    if (from != test.expectation.from){
        return std::format("expected {}, got {}", test.expectation.from, from);
    }
    return true;
}







template <typename Case, UnitTestResult (*check)(const Case&)>
class CaseTest : public UnitTest{
public:
    CaseTest(std::string_view kind, Case test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryStarts {} - {}", kind, test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }
private:
    Case m_case;
};

} // namespace


void add_tests_AutoStoryStarts(UnitTestDatabase& database){
    for (const TableCase& test : TABLE_CASES){
        database.add<CaseTest<TableCase, check_table>>("table", test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
