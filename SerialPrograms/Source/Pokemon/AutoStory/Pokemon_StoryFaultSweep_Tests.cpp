/*  Story Fault Sweep Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The shared sweep against a fake game: two rows, injectors that report
 *  what a test sets, and a known-failing list.
 *
 */

#include <format>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon_StoryFaultSweep.h"
#include "Pokemon_StoryFaultSweep_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

constexpr std::string_view PREFIX = "Fake::AutoStory";

//  An injector that reports `result` for every row.
StoryFaultSweep::Injector reporting(FaultSweepResult result){
    return [result = std::move(result)](Logger&, const std::string&){
        return result;
    };
}

FaultSweepResult clean_result(){
    return FaultSweepResult{.variants = 3, .failures = {}, .not_swept = std::nullopt};
}

FaultSweepResult failing_result(){
    return FaultSweepResult{
        .variants = 3,
        .failures = {{.step = "fly_celadon", .variant = "warp 1", .stopped = "ended in Lavender Town"}},
        .not_swept = std::nullopt,
    };
}

//  A fake game's sweep: rows 01.01 and 01.02, the given injectors.
StoryFaultSweep fake_sweep(std::map<FaultClass, StoryFaultSweep::Injector> injectors, std::map<std::string, std::string> known_failing = {}){
    return StoryFaultSweep{
        .test_prefix = std::string(PREFIX),
        .rows = {"01.01", "01.02"},
        .injectors = std::move(injectors),
        .known_failing = std::move(known_failing),
    };
}


//  The tests add_fault_sweep_tests() makes, by name.
struct SweepNamesTestCase{
    struct Input{
        std::vector<FaultClass> injected;
    };

    struct Expectation{
        std::vector<std::string> names;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

class Test_SweepNames : public UnitTest{
public:
    explicit Test_SweepNames(SweepNamesTestCase test_case)
        : UnitTest(std::format("Pokemon::StoryFaultSweep - tests: {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::map<FaultClass, StoryFaultSweep::Injector> injectors;
        for (FaultClass fault : m_case.input.injected){
            injectors.emplace(fault, reporting(clean_result()));
        }
        std::vector<std::string> names;
        for (const FaultSweepCell& cell : fault_sweep_cells(fake_sweep(std::move(injectors)))){
            names.emplace_back(fault_sweep_test_name(PREFIX, cell));
        }
        if (names != m_case.expectation.names){
            std::string got;
            for (const std::string& name : names){
                got += std::format("\n  {}", name);
            }
            return std::format("Unexpected tests:{}", got);
        }
        return true;
    }

private:
    const SweepNamesTestCase m_case;
};

const SweepNamesTestCase SWEEP_NAMES_CASES[] = {
    {
        .name = "classes in the list's order, then rows",
        .input = {.injected = {FaultClass::LATE_FADE, FaultClass::WARP_NO_OP}},
        .expectation = {.names = {
            "Fake::AutoStory - Fault sweep: 01.01, warp_no_op",
            "Fake::AutoStory - Fault sweep: 01.02, warp_no_op",
            "Fake::AutoStory - Fault sweep: 01.01, late_fade",
            "Fake::AutoStory - Fault sweep: 01.02, late_fade",
        }},
    },
    {
        .name = "a sim fault named by its story class",
        .input = {.injected = {FaultClass::DARK_CAVE}},
        .expectation = {.names = {
            "Fake::AutoStory - Fault sweep: 01.01, dark_map",
            "Fake::AutoStory - Fault sweep: 01.02, dark_map",
        }},
    },
    {
        .name = "no injectors",
        .input = {.injected = {}},
        .expectation = {.names = {}},
    },
};


//  One sweep test made by add_fault_sweep_tests(), run.
struct SweepRunTestCase{
    struct Input{
        FaultSweepResult result;
        //  The test's known-failing fix, if it is on the list.
        std::optional<std::string> known_failing;
    };

    struct Expectation{
        bool passes;
        //  What the failure message starts with, when it fails.
        std::string message;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

class Test_SweepRun : public UnitTest{
public:
    explicit Test_SweepRun(SweepRunTestCase test_case)
        : UnitTest(std::format("Pokemon::StoryFaultSweep - run: {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        const std::string test_name = "Fake::AutoStory - Fault sweep: 01.02, warp_no_op";
        std::map<std::string, std::string> known_failing;
        if (m_case.input.known_failing){
            known_failing.emplace(test_name, *m_case.input.known_failing);
        }
        UnitTestDatabase database;
        add_fault_sweep_tests(database, std::make_shared<const StoryFaultSweep>(fake_sweep(
            {{FaultClass::WARP_NO_OP, reporting(m_case.input.result)}}, std::move(known_failing)
        )));
        const auto test = database.find(test_name);
        if (test == database.end()){
            return std::format("No test \"{}\".", test_name);
        }
        const UnitTestResult result = test->second->run(logger, scope);
        const bool passed = result.result == UnitTestResult::PASSED;
        if (passed != m_case.expectation.passes){
            return std::format("Expected {}, got: {}", m_case.expectation.passes ? "a pass" : "a failure", result.message);
        }
        if (!passed && !result.message.starts_with(m_case.expectation.message)){
            return std::format("Expected a message starting \"{}\", got \"{}\"", m_case.expectation.message, result.message);
        }
        return true;
    }

private:
    const SweepRunTestCase m_case;
};

const SweepRunTestCase SWEEP_RUN_CASES[] = {
    {
        .name = "every variant passes",
        .input = {.result = clean_result(), .known_failing = std::nullopt},
        .expectation = {.passes = true, .message = ""},
    },
    {
        .name = "a variant fails",
        .input = {.result = failing_result(), .known_failing = std::nullopt},
        .expectation = {
            .passes = false,
            .message = "1 of 3 variants fail 01.02 (class=warp_no_op):\n  fly_celadon warp 1: ended in Lavender Town",
        },
    },
    {
        .name = "a row not swept",
        .input = {.result = FaultSweepResult{.variants = 0, .failures = {}, .not_swept = "the row fails clean"}, .known_failing = std::nullopt},
        .expectation = {.passes = true, .message = ""},
    },
    {
        .name = "a known failure",
        .input = {.result = failing_result(), .known_failing = "check the fly lands"},
        .expectation = {.passes = true, .message = ""},
    },
    {
        .name = "a known failure that passes now",
        .input = {.result = clean_result(), .known_failing = "check the fly lands"},
        .expectation = {.passes = false, .message = "on KNOWN_FAILING (check the fly lands) but passes now: take it off the list."},
    },
};


//  The sweep's list checks against the shared classes.
struct SweepListTestCase{
    struct Input{
        std::vector<FaultClass> injected;
        std::vector<std::string> known_failing;
    };

    struct Expectation{
        std::vector<StoryFailureClass> missed;
        std::vector<std::string> stale_known_failing;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

class Test_SweepList : public UnitTest{
public:
    explicit Test_SweepList(SweepListTestCase test_case)
        : UnitTest(std::format("Pokemon::StoryFaultSweep - list: {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::map<FaultClass, StoryFaultSweep::Injector> injectors;
        for (FaultClass fault : m_case.input.injected){
            injectors.emplace(fault, reporting(clean_result()));
        }
        std::map<std::string, std::string> known_failing;
        for (const std::string& name : m_case.input.known_failing){
            known_failing.emplace(name, "a fix");
        }
        const StoryFaultSweep sweep = fake_sweep(std::move(injectors), std::move(known_failing));
        if (classes_not_swept(sweep) != m_case.expectation.missed){
            return std::format("Unexpected classes not swept: {}", classes_not_swept(sweep).size());
        }
        if (known_failing_without_a_test(sweep) != m_case.expectation.stale_known_failing){
            return std::format("Unexpected stale known-failing names: {}", known_failing_without_a_test(sweep).size());
        }
        return true;
    }

private:
    const SweepListTestCase m_case;
};

using C = StoryFailureClass;

const SweepListTestCase SWEEP_LIST_CASES[] = {
    {
        .name = "a game that injects all but one",
        .input = {
            .injected = {
                FaultClass::SCENE_END_POSE, FaultClass::SCENE_EARLY, FaultClass::BATTLE_OPENING, FaultClass::WILD_BATTLES,
                FaultClass::LATE_FADE, FaultClass::SLOW_FADE_IN, FaultClass::WARP_NO_OP, FaultClass::DARK_CAVE,
                FaultClass::LOST_TAP, FaultClass::UNSEEN_STEP,
            },
            .known_failing = {},
        },
        .expectation = {.missed = {C::MOUNT_LOST}, .stale_known_failing = {}},
    },
    {
        .name = "a game that injects none",
        .input = {.injected = {}, .known_failing = {}},
        .expectation = {
            .missed = {
                C::LOST_TAP, C::UNSEEN_STEP, C::MOUNT_LOST, C::WARP_NO_OP, C::LATE_FADE, C::SLOW_FADE_IN,
                C::DARK_MAP, C::SCENE_END_POSE, C::SCENE_EARLY, C::BATTLE_OPENING, C::WILD_BATTLES,
            },
            .stale_known_failing = {},
        },
    },
    {
        .name = "a known failure under an old name",
        .input = {
            .injected = {FaultClass::WARP_NO_OP},
            .known_failing = {
                "Fake::AutoStory - Fault sweep: 01.01, warp_no_op",
                "Fake::AutoStory - Fault sweep: 01.01, warp no-op",
            },
        },
        .expectation = {
            .missed = {
                C::LOST_TAP, C::UNSEEN_STEP, C::MOUNT_LOST, C::LATE_FADE, C::SLOW_FADE_IN,
                C::DARK_MAP, C::SCENE_END_POSE, C::SCENE_EARLY, C::BATTLE_OPENING, C::WILD_BATTLES,
            },
            .stale_known_failing = {"Fake::AutoStory - Fault sweep: 01.01, warp no-op"},
        },
    },
};

} // namespace


void add_tests_StoryFaultSweep(UnitTestDatabase& database){
    for (const SweepNamesTestCase& test_case : SWEEP_NAMES_CASES){
        database.add<Test_SweepNames>(test_case);
    }
    for (const SweepRunTestCase& test_case : SWEEP_RUN_CASES){
        database.add<Test_SweepRun>(test_case);
    }
    for (const SweepListTestCase& test_case : SWEEP_LIST_CASES){
        database.add<Test_SweepList>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
