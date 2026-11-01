/*  BDSP AutoStory Story Tables Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "StorySegments/PokemonBDSP_AutoStory_Segment_00.h"
#include "StorySegments/PokemonBDSP_AutoStory_Segment_01.h"
#include "Utils/PokemonBDSP_AutoStory_StoryTables.h"
#include "PokemonBDSP_AutoStory_StoryTables_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  Builds tables without a console; nothing in them runs.
class DescribeOnlyRunner : public StoryStepRunner{
public:
    virtual bool saved_at_start(const std::string&) const override{
        return false;
    }
    virtual Pokemon::Outcome save_at_start(const std::string&) override{
        return Pokemon::outcome_failed("not_run");
    }
    virtual Pokemon::Outcome run_step(const std::string&, StoryFailure, const StoryStep&) override{
        return Pokemon::outcome_failed("not_run");
    }
};

struct TableCase{
    std::string name;
    std::function<Pokemon::Segment(StoryStepRunner&)> make;
    std::string segment_id;
    std::vector<std::string> checkpoint_ids;
    //  Rows whose first event is not the save (they neither save nor retry).
    std::vector<std::string> unsaved;
};

const std::vector<TableCase>& table_cases(){
    static const std::vector<TableCase> cases{
        {
            .name = "Segment 00",
            .make = make_segment_00,
            .segment_id = "00",
            .checkpoint_ids = {"00.00", "00.01", "00.02", "00.03", "00.04", "00.04b", "00.04c", "00.04d", "00.05"},
            .unsaved = {"00.00"},
        },
        {
            .name = "Segment 01",
            .make = make_segment_01,
            .segment_id = "01",
            .checkpoint_ids = {"01.01", "01.02", "01.03", "01.04", "01.05", "01.06", "01.07", "01.08", "01.09", "01.10", "01.11"},
            .unsaved = {},
        },
    };
    return cases;
}

//  An empty string when `table` matches `expected`; else what differs.
std::string check_table(Pokemon::Segment& table, const TableCase& expected){
    if (table.id != expected.segment_id){
        return std::format("{}: id is \"{}\", expected \"{}\".", expected.name, table.id, expected.segment_id);
    }
    std::vector<std::string> ids;
    for (const Pokemon::CheckpointSpec& spec : table.checkpoints){
        ids.push_back(spec.id);
    }
    if (ids != expected.checkpoint_ids){
        std::string read;
        for (const std::string& id : ids){
            read += " " + id;
        }
        return std::format("{}: {} rows,{}; expected {}.", expected.name, ids.size(), read, expected.checkpoint_ids.size());
    }
    for (Pokemon::CheckpointSpec& spec : table.checkpoints){
        Pokemon::Checkpoint checkpoint = spec.build();
        if (checkpoint.id != spec.id){
            return std::format("{}: row {} builds checkpoint \"{}\".", expected.name, spec.id, checkpoint.id);
        }
        if (checkpoint.events.empty()){
            return std::format("{}: {} has no events.", expected.name, spec.id);
        }
        const bool unsaved = std::find(expected.unsaved.begin(), expected.unsaved.end(), spec.id) != expected.unsaved.end();
        const bool saves_first = checkpoint.events[0].name == "save";
        if (saves_first == unsaved){
            return std::format("{}: {}'s first event is \"{}\", expected {}.",
                expected.name, spec.id, checkpoint.events[0].name, unsaved ? "a step" : "the save");
        }
        const std::vector<std::string> problems = Pokemon::lint_checkpoint(checkpoint);
        if (!problems.empty()){
            return std::format("{}: {} lint: {}", expected.name, spec.id, problems[0]);
        }
    }
    return "";
}

class Test_StoryTables : public UnitTest{
public:
    Test_StoryTables()
        : UnitTest("PokemonBDSP::AutoStory - Story tables: rows, events and describe()")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        for (const TableCase& expected : table_cases()){
            DescribeOnlyRunner runner;
            Pokemon::Segment table = expected.make(runner);
            const std::string problem = check_table(table, expected);
            if (!problem.empty()){
                return problem;
            }
            const std::string text = Pokemon::describe(table);
            for (const std::string& id : expected.checkpoint_ids){
                if (text.find(id) == std::string::npos){
                    return std::format("{}: describe() does not name {}:\n{}", expected.name, id, text);
                }
            }
            logger.log(text);
        }
        return true;
    }
};

}


void add_tests_AutoStory_StoryTables(UnitTestDatabase& database){
    database.add<Test_StoryTables>();
}


}
}
}
