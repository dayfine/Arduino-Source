/*  BDSP AutoStory Leg Trace Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The leg records (Pokemon_LegTrace.h) the story's walks write when a row
 *  runs on the simulation are the route the console should walk too:
 *  devtools/maps/leg_trace.py compares a console log's records with them.
 *  The golden copy is checked in beside this file, so a change to the
 *  route shows as its diff in review.
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/Navigation/Pokemon_LegTrace.h"
#include "PokemonBDSP_AutoStory_LegTrace_Tests.h"
#include "PokemonBDSP_AutoStory_StorySim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

//  The rows with a golden trace.
const std::string TRACED_ROW = "00.04";

//  The leg records' JSON of row `row` run alone on the simulation, or the
//  run's failure.
struct SimTrace{
    std::vector<std::string> legs;
    std::string error;
};
SimTrace sim_leg_trace(Logger& logger, const std::string& row){
    SimExecutor executor(logger);
    const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = row, .to = row});
    SimTrace out;
    if (!report.completed){
        out.error = "the row failed on the simulation: " + executor.result().error;
    }
    const std::string tag = std::string(Pokemon::LEG_TRACE_TAG);
    for (const std::string& note : executor.notes()){
        if (note.starts_with(tag)){
            out.legs.push_back(note.substr(tag.size()));
        }
    }
    return out;
}

//  The trace file's text, a leg a line, as devtools/maps/leg_trace.py reads
//  it ({"about", "legs"}).
std::string trace_text(const std::string& row, const std::vector<std::string>& legs){
    std::string out = "{\n";
    out += std::format("  \"about\": \"Leg records of BDSP AutoStory row {} run on the grid simulation "
        "(Simulation/PokemonBDSP_AutoStory_LegTrace_Tests.cpp). Rewrite with PA_UPDATE_LEG_TRACE=1.\",\n", row);
    out += "  \"legs\": [\n";
    for (size_t i = 0; i < legs.size(); i++){
        out += "    " + legs[i] + (i + 1 < legs.size() ? ",\n" : "\n");
    }
    return out + "  ]\n}\n";
}

//  The golden file, beside this file.
std::string golden_path(const std::string& row){
    const std::string self = __FILE__;
    const size_t slash = self.find_last_of("/\\");
    return (slash == std::string::npos ? std::string() : self.substr(0, slash + 1)) + "leg_trace_" + row + ".golden.json";
}

//  The first line where `a` and `b` differ, both without carriage returns.
std::string first_difference(const std::string& golden, const std::string& traced){
    size_t line = 1;
    size_t begin = 0;
    while (begin < golden.size() || begin < traced.size()){
        const size_t end_a = std::min(golden.find('\n', begin), golden.size());
        const size_t end_b = std::min(traced.find('\n', begin), traced.size());
        const std::string a = begin < golden.size() ? golden.substr(begin, end_a - begin) : "";
        const std::string b = begin < traced.size() ? traced.substr(begin, end_b - begin) : "";
        if (a != b){
            return std::format("line {}:\n--- golden: {}\n+++ traced: {}", line, a, b);
        }
        begin = end_a + 1;
        line++;
    }
    return "";
}

UnitTestResult test_golden_trace(Logger& logger){
    const SimTrace trace = sim_leg_trace(logger, TRACED_ROW);
    if (!trace.error.empty()){
        return trace.error;
    }
    if (trace.legs.empty()){
        return std::format("row {} wrote no leg records.", TRACED_ROW);
    }
    const std::string text = trace_text(TRACED_ROW, trace.legs);
    const std::string path = golden_path(TRACED_ROW);
    // code-health: allow char_pointer -- std::getenv returns a const char pointer
    const char* update = std::getenv("PA_UPDATE_LEG_TRACE");
    if (update != nullptr && update[0] != '\0' && update[0] != '0'){
        std::string crlf;
        for (char ch : text){
            crlf += ch == '\n' ? std::string("\r\n") : std::string(1, ch);
        }
        FileIO file;
        if (!file.open(path, FileMode::WRITE | FileMode::BINARY) || file.write(crlf) != crlf.size()){
            return "Could not write " + path + ".";
        }
        logger.log("Wrote " + path + ".");
        return true;
    }
    std::string golden;
    if (!file_to_string(path, golden)){
        return "Could not read " + path + ".";
    }
    std::erase(golden, '\r');
    const std::string difference = first_difference(golden, text);
    if (difference.empty()){
        return true;
    }
    return "The simulation's leg records differ from " + path + ". If the route change is meant, rerun with "
        "PA_UPDATE_LEG_TRACE=1 and review the file's diff. First difference at " + difference;
}

UnitTestResult test_stable_trace(Logger& logger){
    const SimTrace first = sim_leg_trace(logger, TRACED_ROW);
    const SimTrace second = sim_leg_trace(logger, TRACED_ROW);
    if (first.legs == second.legs){
        return true;
    }
    return "Two runs of the same row wrote different leg records: " + first_difference(
        trace_text(TRACED_ROW, first.legs), trace_text(TRACED_ROW, second.legs));
}


struct LegTraceInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};
struct LegTraceExpectation{
};
struct LegTraceCase{
    LegTraceInput input;
    LegTraceExpectation expectation;
};

class Test_AutoStoryLegTrace : public UnitTest{
public:
    explicit Test_AutoStoryLegTrace(LegTraceCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - " + test_case.input.name)
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
    LegTraceCase m_case;
};

const LegTraceCase LEG_TRACE_CASES[] = {
    {.input = {"Leg trace: 00.04 on the simulation writes leg_trace_00.04.golden.json", test_golden_trace}, .expectation = {}},
    {.input = {"Leg trace: 00.04 on the simulation writes the same records every run", test_stable_trace}, .expectation = {}},
};

} // namespace


void add_tests_AutoStory_LegTrace(UnitTestDatabase& database){
    for (const auto& item : LEG_TRACE_CASES){
        database.add<Test_AutoStoryLegTrace>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
