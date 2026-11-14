/*  Story Fault Sweep
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <set>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryFaultSweep.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  Whether `cell` passes with `result`: logs a table row per failing
//  variant, with the class's tag.
UnitTestResult judge_cell(Logger& logger, const FaultSweepCell& cell, const FaultSweepResult& result){
    const std::string tag = story_failure_class_tag(cell.failure_class);
    if (result.not_swept){
        logger.log(std::format("Fault sweep {} ({}): not swept, {}", cell.row, tag, *result.not_swept));
        return true;
    }
    logger.log(std::format("Fault sweep {} ({}): {} variants, {} fail.", cell.row, tag, result.variants, result.failures.size()));
    if (result.failures.empty()){
        return true;
    }
    std::string message = std::format("{} of {} variants fail {} ({}):", result.failures.size(), result.variants, cell.row, tag);
    for (const FaultSweepFailure& failure : result.failures){
        logger.log(std::format("| {} | {} | {} | {} | {} |", cell.row, failure.step, tag, failure.variant, failure.stopped));
        message += std::format("\n  {} {}: {}", failure.step, failure.variant, failure.stopped);
    }
    return message;
}

//  `result` turned over when `name` is on the known-failing list.
UnitTestResult judge_known_failing(Logger& logger, const StoryFaultSweep& sweep, const std::string& name, UnitTestResult result){
    const auto known = sweep.known_failing.find(name);
    if (known == sweep.known_failing.end()){
        return result;
    }
    if (result.result == UnitTestResult::PASSED){
        return std::format("on KNOWN_FAILING ({}) but passes now: take it off the list.", known->second);
    }
    logger.log(std::format("Known failing until {}: {}", known->second, result.message));
    return true;
}

class Test_FaultSweepCell : public UnitTest{
public:
    Test_FaultSweepCell(std::shared_ptr<const StoryFaultSweep> sweep, FaultSweepCell cell)
        : UnitTest(fault_sweep_test_name(sweep->test_prefix, cell))
        , m_sweep(std::move(sweep))
        , m_cell(std::move(cell))
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        UnitTestResult result;
        try{
            const FaultSweepResult swept = m_sweep->injectors.at(m_cell.fault)(logger, m_cell.row);
            result = judge_cell(logger, m_cell, swept);
        }catch (Exception& e){
            result = std::format("{}: {}", e.name(), e.message());
        }
        return judge_known_failing(logger, *m_sweep, name(), std::move(result));
    }

private:
    const std::shared_ptr<const StoryFaultSweep> m_sweep;
    const FaultSweepCell m_cell;
};

class Test_KnownFailingNames : public UnitTest{
public:
    explicit Test_KnownFailingNames(std::shared_ptr<const StoryFaultSweep> sweep)
        : UnitTest(std::format("{} - Fault sweep: known failing names its tests", sweep->test_prefix))
        , m_sweep(std::move(sweep))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::string> stale = known_failing_without_a_test(*m_sweep);
        if (stale.empty()){
            return true;
        }
        std::string message = "KNOWN_FAILING names no test (renamed?):";
        for (const std::string& name : stale){
            message += std::format("\n  {}", name);
        }
        return message;
    }

private:
    const std::shared_ptr<const StoryFaultSweep> m_sweep;
};

} // namespace


std::vector<FaultSweepCell> fault_sweep_cells(const StoryFaultSweep& sweep){
    std::vector<FaultSweepCell> cells;
    for (const StoryFailureClassInfo& info : story_failure_classes()){
        for (FaultClass fault : info.injected_by){
            if (!sweep.injectors.contains(fault)){
                continue;
            }
            for (const std::string& row : sweep.rows){
                cells.push_back(FaultSweepCell{.failure_class = info.failure_class, .fault = fault, .row = row});
            }
        }
    }
    return cells;
}

std::string fault_sweep_test_name(std::string_view test_prefix, const FaultSweepCell& cell){
    return std::format("{} - Fault sweep: {}, {}", test_prefix, cell.row, story_failure_class_info(cell.failure_class).id);
}

std::vector<StoryFailureClass> classes_not_swept(const StoryFaultSweep& sweep){
    std::vector<StoryFailureClass> missed;
    for (const StoryFailureClassInfo& info : story_failure_classes()){
        for (FaultClass fault : info.injected_by){
            if (!sweep.injectors.contains(fault)){
                missed.push_back(info.failure_class);
                break;
            }
        }
    }
    return missed;
}

std::vector<std::string> known_failing_without_a_test(const StoryFaultSweep& sweep){
    std::set<std::string> names;
    for (const FaultSweepCell& cell : fault_sweep_cells(sweep)){
        names.insert(fault_sweep_test_name(sweep.test_prefix, cell));
    }
    std::vector<std::string> stale;
    for (const auto& item : sweep.known_failing){
        if (!names.contains(item.first)){
            stale.push_back(item.first);
        }
    }
    return stale;
}

void add_fault_sweep_tests(UnitTestDatabase& database, std::shared_ptr<const StoryFaultSweep> sweep){
    for (FaultSweepCell& cell : fault_sweep_cells(*sweep)){
        database.add<Test_FaultSweepCell>(sweep, std::move(cell));
    }
    database.add<Test_KnownFailingNames>(std::move(sweep));
}



} // namespace Pokemon
} // namespace PokemonAutomation
