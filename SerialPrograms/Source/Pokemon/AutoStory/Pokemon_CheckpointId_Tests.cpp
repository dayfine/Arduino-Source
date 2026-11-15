/*  AutoStory Checkpoint Id Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <compare>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Pokemon_CheckpointId.h"
#include "Pokemon_CheckpointId_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{


//  Parse: text in, the id printed back out (nullopt when parse rejects it).

struct ParseInput{
    std::string text;
};

struct ParseExpectation{
    std::optional<std::string> id;
    std::optional<std::string> segment;
};

struct ParseScenario{
    std::string name;
    ParseInput input;
    ParseExpectation expectation;
};

class Test_CheckpointIdParse : public UnitTest{
public:
    Test_CheckpointIdParse(ParseScenario scenario)
        : UnitTest(std::format("Pokemon::CheckpointId::parse - {}", scenario.name))
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<CheckpointId> got = CheckpointId::parse(m_scenario.input.text);
        const ParseExpectation& expected = m_scenario.expectation;
        if (!expected.id){
            if (got){
                return std::format("Expected \"{}\" to be rejected, got {}", m_scenario.input.text, got->to_string());
            }
            return true;
        }
        if (!got){
            return std::format("Expected \"{}\" to parse, it was rejected", m_scenario.input.text);
        }
        if (got->to_string() != *expected.id){
            return std::format("Expected id {}, got {}", *expected.id, got->to_string());
        }
        if (got->segment_string() != *expected.segment){
            return std::format("Expected segment {}, got {}", *expected.segment, got->segment_string());
        }
        return true;
    }

private:
    const ParseScenario m_scenario;
};


//  Ordering: two ids, how the first compares to the second.

struct OrderInput{
    CheckpointId left;
    CheckpointId right;
};

struct OrderExpectation{
    std::strong_ordering order;
};

struct OrderScenario{
    std::string name;
    OrderInput input;
    OrderExpectation expectation;
};

const char* order_name(std::strong_ordering order){
    if (order < 0){
        return "less";
    }
    if (order > 0){
        return "greater";
    }
    return "equal";
}

class Test_CheckpointIdOrder : public UnitTest{
public:
    Test_CheckpointIdOrder(OrderScenario scenario)
        : UnitTest(std::format("Pokemon::CheckpointId order - {}", scenario.name))
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::strong_ordering got = m_scenario.input.left <=> m_scenario.input.right;
        if (got != m_scenario.expectation.order){
            return std::format(
                "Expected {} to be {} than {}, got {}",
                m_scenario.input.left.to_string(),
                order_name(m_scenario.expectation.order),
                m_scenario.input.right.to_string(),
                order_name(got)
            );
        }
        return true;
    }

private:
    const OrderScenario m_scenario;
};


//  Segment starts: the checkpoints a run passes, in order, and the ones that
//  start a segment.

struct SegmentStartInput{
    std::vector<CheckpointId> passed;
};

struct SegmentStartExpectation{
    std::vector<std::string> starts;
};

struct SegmentStartScenario{
    std::string name;
    SegmentStartInput input;
    SegmentStartExpectation expectation;
};

std::string join(const std::vector<std::string>& ids){
    std::string ret;
    for (const std::string& id : ids){
        if (!ret.empty()){
            ret += ", ";
        }
        ret += id;
    }
    return ret;
}

class Test_CheckpointIdSegmentStart : public UnitTest{
public:
    Test_CheckpointIdSegmentStart(SegmentStartScenario scenario)
        : UnitTest(std::format("Pokemon::CheckpointId starts_segment - {}", scenario.name))
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<std::string> got;
        std::optional<CheckpointId> previous;
        for (CheckpointId next : m_scenario.input.passed){
            if (starts_segment(previous, next)){
                got.emplace_back(next.to_string());
            }
            previous = next;
        }
        if (got != m_scenario.expectation.starts){
            return std::format(
                "Expected segment starts [{}], got [{}]",
                join(m_scenario.expectation.starts), join(got)
            );
        }
        return true;
    }

private:
    const SegmentStartScenario m_scenario;
};


}  // namespace



void add_tests_CheckpointId(UnitTestDatabase& database){
    const std::vector<ParseScenario> parse_cases{
        {.name = "segment and checkpoint",  .input = {.text = "03.08"}, .expectation = {.id = "03.08", .segment = "03"}},
        {.name = "first checkpoint",        .input = {.text = "00.00"}, .expectation = {.id = "00.00", .segment = "00"}},
        {.name = "largest id",              .input = {.text = "99.99"}, .expectation = {.id = "99.99", .segment = "99"}},
        {.name = "letter suffix",           .input = {.text = "03.08b"}},
        {.name = "one-digit segment",       .input = {.text = "3.08"}},
        {.name = "one-digit checkpoint",    .input = {.text = "03.8"}},
        {.name = "three-digit segment",     .input = {.text = "100.08"}},
        {.name = "three-digit checkpoint",  .input = {.text = "03.100"}},
        {.name = "no dot",                  .input = {.text = "0308"}},
        {.name = "other separator",         .input = {.text = "03-08"}},
        {.name = "extra part",              .input = {.text = "03.08.01"}},
        {.name = "segment only",            .input = {.text = "03"}},
        {.name = "sign",                    .input = {.text = "+3.08"}},
        {.name = "spaces",                  .input = {.text = " 3.08"}},
        {.name = "empty",                   .input = {.text = ""}},
    };
    for (const ParseScenario& scenario : parse_cases){
        database.add<Test_CheckpointIdParse>(scenario);
    }

    const std::vector<OrderScenario> order_cases{
        {
            .name = "same id",
            .input = {.left = {3, 8}, .right = {3, 8}},
            .expectation = {.order = std::strong_ordering::equal},
        },
        {
            .name = "later checkpoint in the same segment",
            .input = {.left = {3, 9}, .right = {3, 8}},
            .expectation = {.order = std::strong_ordering::greater},
        },
        {
            .name = "segment wins over checkpoint",
            .input = {.left = {2, 99}, .right = {3, 0}},
            .expectation = {.order = std::strong_ordering::less},
        },
    };
    for (const OrderScenario& scenario : order_cases){
        database.add<Test_CheckpointIdOrder>(scenario);
    }

    const std::vector<SegmentStartScenario> segment_cases{
        {
            .name = "first checkpoint of a run starts its segment",
            .input = {.passed = {{3, 8}}},
            .expectation = {.starts = {"03.08"}},
        },
        {
            .name = "checkpoints in one segment start it once",
            .input = {.passed = {{3, 8}, {3, 9}, {3, 10}}},
            .expectation = {.starts = {"03.08"}},
        },
        {
            .name = "each new segment starts once",
            .input = {.passed = {{2, 4}, {2, 5}, {3, 0}, {3, 1}, {4, 0}}},
            .expectation = {.starts = {"02.04", "03.00", "04.00"}},
        },
        {
            .name = "empty run starts nothing",
            .input = {.passed = {}},
            .expectation = {.starts = {}},
        },
    };
    for (const SegmentStartScenario& scenario : segment_cases){
        database.add<Test_CheckpointIdSegmentStart>(scenario);
    }
}



}  // namespace Pokemon
}  // namespace PokemonAutomation
