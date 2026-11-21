/*  PokemonLZA AutoStory - Screen Events Tests
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
#include "PokemonLZA_AutoStoryScreenEvents.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStory_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

namespace{

struct FrameCase{
    std::string name;
    struct Input{
        std::string_view path;  //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        std::optional<bool> interact_prompt = std::nullopt;
        std::optional<bool> dialog = std::nullopt;
        std::optional<bool> choice = std::nullopt;
        std::optional<bool> fade_white = std::nullopt;
    };
    Expectation expectation;
};

const FrameCase FRAME_CASES[] = {
    {
        //  01.02: no Pokemon, no party bar.
        .name = "the A Talk prompt on the Italian game during 01.02",
        .input = {.path = "AutoStoryScreenEvents/it_0102_parla_talk_prompt.png"},
        .expectation = {.interact_prompt = true, .dialog = false, .choice = false},
    },
    {
        .name = "the tourist's yes or yes dialog and choice box during 01.01 arrival",
        .input = {.path = "AutoStoryDetectors/ArrivalToAd/0065-text_box+choice_box.jpg"},
        .expectation = {.dialog = true, .choice = true, .fade_white = false},
    },
    {
        .name = "the bag's choice box during 01.01 arrival",
        .input = {.path = "AutoStoryDetectors/ArrivalToMissionBanner/0173-text_box.jpg"},
        .expectation = {.interact_prompt = false, .dialog = true, .choice = true, .fade_white = false},
    },
    {
        .name = "the white fade during 01.01 arrival",
        .input = {.path = "AutoStoryDetectors/ArrivalPastMissionBanner/0008-fade_white.jpg"},
        .expectation = {.dialog = false, .choice = false, .fade_white = true},
    },
    {
        .name = "light pavement with autosave during 01.01 arrival",
        .input = {.path = "AutoStoryDetectors/ArrivalToAd/0038-nothing+autosave.jpg"},
        .expectation = {.interact_prompt = false, .dialog = false, .choice = false, .fade_white = false},
    },
    {
        .name = "a plain dialog box without choices during 01.01 arrival",
        .input = {.path = "AutoStoryDetectors/ArrivalPastMissionBanner/0015-text_box.jpg"},
        .expectation = {.interact_prompt = false, .dialog = true, .choice = false, .fade_white = false},
    },
};

UnitTestResult check_frame(const FrameCase& test){
    const std::string path = std::format("{}PokemonLZA/{}", UNIT_TEST_RESOURCE_PATH(), test.input.path);
    if (!Filesystem::exists(Filesystem::Path(path))){
        return std::format("{} is missing.", path);
    }
    PlzaScreenObserver observer;
    const ScreenEvents events = observer.read(ImageRGB32(path));
    const std::pair<std::string_view, std::pair<ScreenEvent, std::optional<bool>>> checks[] = {
        {"A prompt", {ScreenEvent::INTERACT_PROMPT, test.expectation.interact_prompt}},
        {"dialog", {ScreenEvent::DIALOG_BOX, test.expectation.dialog}},
        {"choice", {ScreenEvent::CHOICE_BOX, test.expectation.choice}},
        {"white fade", {ScreenEvent::FADE_WHITE, test.expectation.fade_white}},
    };
    for (const auto& [label, check] : checks){
        if (!check.second){
            continue;
        }
        const bool got = events.has(check.first);
        if (got != *check.second){
            return std::format(
                "{}: expected {} {}, got {}", test.input.path, label,
                *check.second ? "yes" : "no", got ? "yes" : "no"
            );
        }
    }
    return true;
}

using Pokemon::StoryCheckpoint;

std::optional<size_t> index_of(const std::vector<StoryCheckpoint>& rows, std::string_view id){
    for (size_t index = 0; index < rows.size(); index++){
        if (rows[index].id == id){
            return index;
        }
    }
    return std::nullopt;
}

//  A checkpoint id, or "stop" for start_position_unknown.
std::string run_start_id(
    const std::vector<StoryCheckpoint>& rows, const std::vector<CheckpointStart>& starts,
    std::string_view asked, const StartSignature& read
){
    const std::optional<size_t> asked_index = index_of(rows, asked);
    if (!asked_index){
        return std::format("no checkpoint {}", asked);
    }
    const std::optional<size_t> from = checkpoint_at(rows, starts, *asked_index, read);
    return from ? rows[*from].id : std::string("stop");
}

//  A made-up table, for the rules of checkpoint_at().
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
        .input = {.starts = {{.checkpoint = "B", .signature = {.party = 1}}}, .asked = "B", .read = AT_COUNTER},
        .expectation = {.from = "stop"},
    },
};

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
    CaseTest(std::string_view prefix, const Case& test_case)
        : UnitTest(std::format("{}{}", prefix, test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }
private:
    const Case& m_case;
};

} // namespace


void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database){
    for (const FrameCase& test : FRAME_CASES){
        database.add<CaseTest<FrameCase, check_frame>>("PokemonLZA::AutoStoryScreenEvents - console frame: ", test);
    }
    for (const TableCase& test : TABLE_CASES){
        database.add<CaseTest<TableCase, check_table>>("PokemonLZA::AutoStoryStarts table - ", test);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
