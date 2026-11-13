/*  BDSP AutoStory - Sim golden tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story simulation (SimExecutor on GridSim) does with each row,
 *  kept as goldens so that the shared sim core (seam 3) can move BDSP's
 *  sim onto it and show that nothing changed:
 *
 *  - sim_golden_rows.txt: each row the fault sweep runs (FaultSweep_Tests'
 *    FIRST_ROW to LAST_ROW), run clean on its own: whether it completed or
 *    where it stopped, where the player ended, the steps the sim stubbed,
 *    the walks that end where nothing says, the scenes a walk fought, the
 *    fades, banners, doors and tap steps it looked for, and every line its
 *    walks wrote (the route walker's and the leg records).
 *  - sim_golden_faults.txt: the fault sweep's outcome for each row and
 *    fault class: its result and its summary lines. The segment runner's
 *    lines of each variant are left out: they repeat the rows golden.
 *
 *  Rewrite both with PA_UPDATE_SIM_GOLDENS=1 (Pokemon_SimGolden.h).
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/Navigation/Simulation/Pokemon_SimGolden.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_FaultSweep_Tests.h"
#include "PokemonBDSP_AutoStory_StorySim.h"
#include "PokemonBDSP_AutoStory_SimGolden_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  The rows the goldens run: the fault sweep's (FaultSweep_Tests.cpp).
constexpr std::string_view FIRST_ROW = "00.00";
constexpr std::string_view LAST_ROW = "01.11";

//  Drops the lines it is given: the segment runner's, which the goldens
//  leave out.
class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

//  Keeps the lines it is given.
class LineLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string& msg, Color) override{
        lines.push_back(msg);
    }
    std::vector<std::string> lines;
};

//  `text`'s lines, each indented by `indent`.
void add_indented(const std::string& text, const std::string& indent, std::vector<std::string>& lines){
    size_t begin = 0;
    while (begin <= text.size()){
        const size_t end = std::min(text.find('\n', begin), text.size());
        lines.push_back(indent + text.substr(begin, end - begin));
        begin = end + 1;
    }
}

//  Row `row` run clean on its own, a line per fact.
void golden_row(const StoryCheckpoint& row, std::vector<std::string>& lines){
    QuietLogger quiet;
    SimExecutor executor(quiet, {.fade = {}, .banner = {}}, {});
    const Pokemon::SegmentReport report = run_rows_on(executor, quiet, {.from = row.id, .to = row.id});
    const SimRun& run = executor.result();
    if (report.completed){
        lines.push_back(row.id + ": completed");
    }else{
        lines.push_back(std::format("{}: stopped in {}: {}", row.id, run.failed_step, run.error));
    }
    lines.push_back("  ends on " + detail::end_of(run));
    for (const SimRun::RowStart& start : run.rows){
        for (const std::string& stubbed : start.stubbed){
            lines.push_back("  stubbed " + stubbed);
        }
    }
    for (const std::string& step : run.unchecked){
        lines.push_back("  no end to check after " + step);
    }
    for (const std::string& scene : run.scenes_fought){
        lines.push_back("  fought scene " + scene);
    }
    const SimMisses& misses = executor.misses();
    lines.push_back(std::format(
        "  looked for {} fades, {} banners, {} doors, {} tap steps",
        misses.fades, misses.banners, misses.doors, misses.tap_steps
    ));
    for (const std::string& note : executor.notes()){
        add_indented(note, "  | ", lines);
    }
}

UnitTestResult test_rows_golden(Logger& logger, CancellableScope&){
    std::vector<std::string> lines;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id >= FIRST_ROW && row.id <= LAST_ROW){
            golden_row(row, lines);
        }
    }
    const std::string text = Pokemon::sim_golden_text(
        "BDSP AutoStory rows on the story simulation, clean (PokemonBDSP_AutoStory_SimGolden_Tests.cpp)", lines
    );
    return Pokemon::check_sim_golden(logger, Pokemon::sim_golden_path(__FILE__, "sim_golden_rows.txt"), text);
}


std::string result_name(UnitTestResult::Result result){
    switch (result){
    case UnitTestResult::PASSED:
        return "passed";
    case UnitTestResult::FAILED:
        return "failed";
    case UnitTestResult::SKIPPED:
        return "skipped";
    default:
        return "not run";
    }
}

//  Whether `line` is one of the sweep's own: its summary, a failed
//  variant, a known failure, or why it did not sweep.
bool sweep_line(std::string_view line){
    return line.starts_with("Fault sweep ") || line.starts_with("| ") || line.starts_with("Known failing ");
}

UnitTestResult test_faults_golden(Logger& logger, CancellableScope& scope){
    UnitTestDatabase sweep;
    add_tests_AutoStory_FaultSweep(sweep);
    std::vector<std::string> lines;
    for (const auto& item : sweep){
        LineLogger captured;
        UnitTestResult result;
        try{
            result = item.second->run(captured, scope);
        }catch (Exception& e){
            result = std::string(e.name()) + ": " + e.message();
        }
        lines.push_back(item.first + ": " + result_name(result.result));
        if (!result.message.empty()){
            add_indented(result.message, "  | ", lines);
        }
        for (const std::string& line : captured.lines){
            if (sweep_line(line)){
                add_indented(line, "  ", lines);
            }
        }
    }
    const std::string text = Pokemon::sim_golden_text(
        "BDSP AutoStory fault sweep outcomes (PokemonBDSP_AutoStory_SimGolden_Tests.cpp)", lines
    );
    return Pokemon::check_sim_golden(logger, Pokemon::sim_golden_path(__FILE__, "sim_golden_faults.txt"), text);
}


struct SimGoldenInput{
    std::string name;
    UnitTestResult (*body)(Logger& logger, CancellableScope& scope);
};

struct SimGoldenExpectation{
    std::string_view description;
};

struct SimGoldenTestCase{
    SimGoldenInput input;
    SimGoldenExpectation expectation;
};

class Test_AutoStorySimGolden : public UnitTest{
public:
    explicit Test_AutoStorySimGolden(SimGoldenTestCase scenario)
        : UnitTest("PokemonBDSP::AutoStory - Sim golden: " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        try{
            return m_scenario.input.body(logger, scope);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    SimGoldenTestCase m_scenario;
};

const SimGoldenTestCase SIM_GOLDEN_CASES[] = {
    {.input = {.name = "rows on the story simulation, clean", .body = test_rows_golden}, .expectation = {.description = "each row run clean matches sim_golden_rows.txt"}},
    {.input = {.name = "fault sweep outcomes", .body = test_faults_golden}, .expectation = {.description = "each fault sweep's outcome matches sim_golden_faults.txt"}},
};

} // namespace


void add_tests_AutoStory_SimGolden(UnitTestDatabase& database){
    for (const SimGoldenTestCase& scenario : SIM_GOLDEN_CASES){
        database.add<Test_AutoStorySimGolden>(scenario);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
