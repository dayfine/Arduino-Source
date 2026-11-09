/*  PokemonLZA AutoStory - Story table Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "PokemonLZA_AutoStoryExecutor.h"
#include "PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStoryTable_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;

//  Does every step it is asked to, writing down "id KIND name".
class RecordingExecutor : public Pokemon::StoryStepExecutor{
public:
    std::vector<std::string> trace;

    virtual void begin_attempt(const StoryCheckpoint& data) override{
        m_checkpoint = data.id;
    }
    virtual Pokemon::Outcome run(Pokemon::RunState&, const StoryStep& step) override{
        trace.push_back(std::format("{} {} {}", m_checkpoint, Pokemon::story_step_kind_name(step.kind), step.name));
        return Pokemon::outcome_ok();
    }

private:
    std::string m_checkpoint;
};

//  The table: ids unique and in order, each row starts somewhere, lints,
//  and every step is one PLZA's executor does.
UnitTestResult test_story_table_lints(Logger&){
    std::string previous;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id <= previous){
            return std::format("{}: repeated or out of order after {}.", row.id, previous);
        }
        previous = row.id;
        if (!row.start){
            return row.id + ": no start; every row starts from a save.";
        }
        const std::vector<std::string> problems = Pokemon::lint_story_checkpoint(row);
        if (!problems.empty()){
            return row.id + ": " + problems[0];
        }
        for (const StoryStep& step : row.steps){
            if (!story_step_supported(step)){
                return std::format(
                    "{} {}: the executor does no {} \"{}\".",
                    row.id, step.name, Pokemon::story_step_kind_name(step.kind), step.variant
                );
            }
        }
    }
    if (story_checkpoints().empty() || story_checkpoints().front().id != "01.01"){
        return "Expected the table to start at 01.01.";
    }
    return true;
}

//  Every move the table names is known, and each runs forward for some time
//  toward a heading on the compass, if it has one.
UnitTestResult test_story_moves_known(Logger&){
    std::set<std::string> used;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (step.kind == StoryStepKind::GAME && step.variant != "choose_starter"){
                used.insert(step.variant);
            }
        }
    }
    for (const std::string& name : used){
        if (!story_moves().contains(name)){
            return "No move named " + name + ".";
        }
    }
    for (const auto& [name, move] : story_moves()){
        if (!used.contains(name)){
            return "Move " + name + " is in no step.";
        }
        if (move.duration.count() <= 0){
            return std::format("Move {} runs for {} ms, expected more than 0.", name, move.duration.count());
        }
        if (move.heading && (*move.heading < 0 || *move.heading >= 360)){
            return std::format("Move {} heads {} deg, expected [0, 360).", name, *move.heading);
        }
    }
    return true;
}

//  Unsupported kinds and variants are refused by name: a tile walk, a fly
//  without "fly", a move nobody defined.
UnitTestResult test_unsupported_steps_refused(Logger&){
    const std::vector<std::pair<StoryStepKind, std::string>> refused{
        {StoryStepKind::LEGS, ""},
        {StoryStepKind::STEP, ""},
        {StoryStepKind::ROUTE, ""},
        {StoryStepKind::MASH, "x"},
        {StoryStepKind::GAME, "no_such_move"},
    };
    for (const auto& [kind, variant] : refused){
        StoryStep step;
        step.kind = kind;
        step.name = "probe";
        step.variant = variant;
        if (story_step_supported(step)){
            return std::format("Expected {} \"{}\" to be refused.", Pokemon::story_step_kind_name(kind), variant);
        }
    }
    return true;
}

//  The whole table through make_story_segment() and the shared runner, each
//  step done: every row passes in order, saving at its end.
UnitTestResult test_story_table_runs(Logger&){
    RecordingExecutor executor;
    Pokemon::Segment segment = make_story_segment(story_checkpoints(), executor);
    size_t saves = 0;
    Pokemon::SegmentHooks hooks;
    hooks.save = [&saves](Pokemon::RunState&){
        saves++;
        return Pokemon::outcome_ok();
    };
    hooks.reload = [](Pokemon::RunState&){
        return Pokemon::outcome_failed("no_reload", "nothing fails here");
    };
    hooks.log = [](const std::string&){};
    Pokemon::RunState state;
    const Pokemon::SegmentReport report = Pokemon::run_segment(state, segment, {}, Pokemon::SegmentPolicy{}, hooks);
    if (!report.completed){
        return std::format("Expected the table to run, stopped at {} {}.", report.failed_checkpoint, report.failed_event);
    }
    std::vector<std::string> expected;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            expected.push_back(std::format("{} {} {}", row.id, Pokemon::story_step_kind_name(step.kind), step.name));
        }
    }
    if (executor.trace != expected){
        return std::format("Expected {} steps in table order, ran {}.", expected.size(), executor.trace.size());
    }
    if (saves != story_checkpoints().size()){
        return std::format("Expected a save per row ({}), saw {}.", story_checkpoints().size(), saves);
    }
    return true;
}

//  The starters' rows in the game's list: top to bottom.
UnitTestResult test_starter_rows(Logger&){
    const std::vector<std::pair<StarterChoice, size_t>> rows{
        {StarterChoice::CHIKORITA, 0},
        {StarterChoice::TEPIG, 1},
        {StarterChoice::TOTODILE, 2},
    };
    for (const auto& [starter, expected] : rows){
        const size_t index = starter_index(starter);
        if (index != expected){
            return std::format("Expected row {}, read {}.", expected, index);
        }
    }
    return true;
}


struct AutoStoryTableInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryTableExpectation{
};

struct AutoStoryTableCase{
    AutoStoryTableInput input;
    AutoStoryTableExpectation expectation;
};

class Test_AutoStoryTable : public UnitTest{
public:
    explicit Test_AutoStoryTable(AutoStoryTableCase test_case)
        : UnitTest("PokemonLZA::AutoStory - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryTableCase m_case;
};

} // namespace


const AutoStoryTableCase AUTO_STORY_TABLE_CASES[] = {
    {
        .input = {"Story table: rows lint, every step is one the executor does", test_story_table_lints},
        .expectation = {},
    },
    {
        .input = {"Story table: every move is known and runs", test_story_moves_known},
        .expectation = {},
    },
    {
        .input = {"Executor: tile walks and unknown variants are refused", test_unsupported_steps_refused},
        .expectation = {},
    },
    {
        .input = {"Story table: runs through the shared engine, saving per row", test_story_table_runs},
        .expectation = {},
    },
    {
        .input = {"Starters: rows top to bottom", test_starter_rows},
        .expectation = {},
    },
};

void add_tests_AutoStory_StoryTable(UnitTestDatabase& database){
    for (const AutoStoryTableCase& item : AUTO_STORY_TABLE_CASES){
        database.add<Test_AutoStoryTable>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
