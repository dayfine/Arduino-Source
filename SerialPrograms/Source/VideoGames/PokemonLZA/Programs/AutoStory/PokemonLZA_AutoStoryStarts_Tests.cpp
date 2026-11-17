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
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
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


//  Runs against the built-in plans' start signatures.
struct BuiltinCase{
    std::string name;
    struct Input{
        std::string asked;
        StartSignature read;
    };
    Input input;
    struct Expectation{
        std::string from;   //  A checkpoint id, or "stop".
    };
    Expectation expectation;
};

const BuiltinCase BUILTIN_CASES[] = {
    {
        //  lab-B, 2026-11-16 13:57: the save at the Center counter, the run
        //  asked for 03.08, and it searched Wild Zone 1 inside the Center.
        .name = "asked 03.08, the screen at the Center counter: from 03.06",
        .input = {.asked = "03.08", .read = {.talk_prompt = true, .wild_zone = false}},
        .expectation = {.from = "03.06"},
    },
    {
        .name = "asked 03.08, the screen at Taunie's rocks in Wild Zone 1: from 03.08",
        .input = {.asked = "03.08", .read = {.talk_prompt = false, .wild_zone = true}},
        .expectation = {.from = "03.08"},
    },
    {
        .name = "asked 03.06, the screen at the Center counter: from 03.06",
        .input = {.asked = "03.06", .read = {.talk_prompt = true, .wild_zone = false}},
        .expectation = {.from = "03.06"},
    },
    {
        .name = "asked 03.08, the screen out on the street: stop",
        .input = {.asked = "03.08", .read = {.talk_prompt = false, .wild_zone = false}},
        .expectation = {.from = "stop"},
    },
    {
        .name = "asked 03.08, nothing read: stop",
        .input = {.asked = "03.08", .read = {}},
        .expectation = {.from = "stop"},
    },
    {
        .name = "asked 03.06, the screen in a wild zone: stop",
        .input = {.asked = "03.06", .read = {.talk_prompt = false, .wild_zone = true}},
        .expectation = {.from = "stop"},
    },
    {
        //  A backup card is read the same way: 1 registered is 03.06's.
        .name = "asked 03.08, the backup card shows 1 registered: from 03.06",
        .input = {.asked = "03.08", .read = {.registered = 1}},
        .expectation = {.from = "03.06"},
    },
    {
        .name = "asked 03.08, an unknown backup card: stop",
        .input = {.asked = "03.08", .read = {.registered = 7}},
        .expectation = {.from = "stop"},
    },    {
        //  lab-B, 2026-11-16 18:34: a save past 03.09, on the street by the
        //  zone wall, read as in a wild zone; 03.08 ran as a no-op and
        //  03.08b walked from the street.
        .name = "asked 03.08, the street with five in the party: stop",
        .input = {.asked = "03.08", .read = {.talk_prompt = false, .wild_zone = true, .party = 5}},
        .expectation = {.from = "stop"},
    },
    {
        .name = "asked 03.08, a catch's autosave mid-zone with four in the party: stop",
        .input = {.asked = "03.08", .read = {.talk_prompt = false, .wild_zone = true, .party = 4}},
        .expectation = {.from = "stop"},
    },
    {
        .name = "asked 03.08, Taunie's rocks with three in the party: from 03.08",
        .input = {.asked = "03.08", .read = {.talk_prompt = false, .wild_zone = true, .party = 3}},
        .expectation = {.from = "03.08"},
    },
    {
        .name = "asked 03.08b, the fifth catch's autosave in the zone: from 03.08b",
        .input = {.asked = "03.08b", .read = {.talk_prompt = false, .wild_zone = true, .party = 5}},
        .expectation = {.from = "03.08b"},
    },
    {
        .name = "asked 03.09, the fifth catch's autosave in the zone: from 03.08b",
        .input = {.asked = "03.09", .read = {.talk_prompt = false, .wild_zone = true, .party = 5}},
        .expectation = {.from = "03.08b"},
    },
    {
        .name = "asked 03.09, the menu save facing the far gate's prompt: from 03.09",
        .input = {.asked = "03.09", .read = {.talk_prompt = true, .wild_zone = true, .party = 5}},
        .expectation = {.from = "03.09"},
    },
    {
        .name = "asked 03.08b, the menu save facing the far gate's prompt: stop",
        .input = {.asked = "03.08b", .read = {.talk_prompt = true, .wild_zone = true, .party = 5}},
        .expectation = {.from = "stop"},
    },
    {
        .name = "asked 03.10, the menu save in the lab lobby: from 03.10",
        .input = {.asked = "03.10", .read = {.talk_prompt = false, .wild_zone = false, .party = 5}},
        .expectation = {.from = "03.10"},
    },
    {
        .name = "asked 03.10, the menu save facing the far gate's prompt: from 03.09",
        .input = {.asked = "03.10", .read = {.talk_prompt = true, .wild_zone = true, .party = 5}},
        .expectation = {.from = "03.09"},
    },
    {
        .name = "asked 03.11, the menu save facing the far gate's prompt: from 03.09",
        .input = {.asked = "03.11", .read = {.talk_prompt = true, .wild_zone = true, .party = 5}},
        .expectation = {.from = "03.09"},
    },
    {
        .name = "asked 03.11, the 3F autosave: from 03.11",
        .input = {.asked = "03.11", .read = {.talk_prompt = false, .wild_zone = false, .party = 5}},
        .expectation = {.from = "03.11"},
    },
};

UnitTestResult check_builtin(const BuiltinCase& test){
    const std::string from = run_start_id(
        builtin_story_data().checkpoints, builtin_checkpoint_starts(), test.input.asked, test.input.read
    );
    if (from != test.expectation.from){
        return std::format("expected {}, got {}", test.expectation.from, from);
    }
    return true;
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
        .name = "asked has no signature, the screen matches an earlier start: from it",
        .input = {.starts = {{.checkpoint = "A", .signature = AT_COUNTER}}, .asked = "C", .read = AT_COUNTER},
        .expectation = {.from = "A"},
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


//  The console's frames, PokemonLZA/AutoStoryStarts/<file> (console B,
//  2026-11-16, Italian), read and placed for a run asked to start at
//  03.08.
struct FrameCase{
    std::string name;
    struct Input{
        std::string file;
    };
    Input input;
    struct Expectation{
        std::string from;   //  A checkpoint id, or "stop".
    };
    Expectation expectation;
};

const FrameCase FRAME_CASES[] = {
    {
        //  lab-B outbox za_bk5_g: "A Parla" at the counter.
        .name = "the Center counter: 03.06",
        .input = {.file = "Italian_center_counter.png"},
        .expectation = {.from = "03.06"},
    },
    {
        //  fails/it_0308_run3_rocks_120: the lead's panel at the bottom left.
        .name = "Taunie's rocks in Wild Zone 1: 03.08",
        .input = {.file = "Italian_zone1_rocks.png"},
        .expectation = {.from = "03.08"},
    },
};

UnitTestResult check_frame(const FrameCase& test){
    const std::string path = std::format("{}PokemonLZA/AutoStoryStarts/{}", UNIT_TEST_RESOURCE_PATH(), test.input.file);
    if (!Filesystem::exists(Filesystem::Path(path))){
        return UnitTestResult(UnitTestResult::SKIPPED, std::format("{} is not in the test images.", test.input.file));
    }
    const StartSignature read = read_start_screen(ImageRGB32(path));
    const std::string from = run_start_id(builtin_story_data().checkpoints, builtin_checkpoint_starts(), "03.08", read);
    if (from != test.expectation.from){
        return std::format("{}: read {}: expected {}, got {}", test.input.file, to_string(read), test.expectation.from, from);
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
    for (const BuiltinCase& test : BUILTIN_CASES){
        database.add<CaseTest<BuiltinCase, check_builtin>>("built-in", test);
    }
    for (const TableCase& test : TABLE_CASES){
        database.add<CaseTest<TableCase, check_table>>("table", test);
    }
    for (const FrameCase& test : FRAME_CASES){
        database.add<CaseTest<FrameCase, check_frame>>("frame", test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
