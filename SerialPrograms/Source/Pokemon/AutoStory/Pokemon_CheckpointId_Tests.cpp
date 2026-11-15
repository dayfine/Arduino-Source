/*  AutoStory Checkpoint Id Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <compare>
#include <format>
#include <optional>
#include <string>
#include <string_view>
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

struct ParsedId{
    std::string id;
    std::string segment;
};

struct ParseExpectation{
    std::optional<ParsedId> parsed;
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
        if (!expected.parsed){
            if (got){
                return std::format("Expected \"{}\" to be rejected, got {}", m_scenario.input.text, got->to_string());
            }
            return true;
        }
        if (!got){
            return std::format("Expected \"{}\" to parse, it was rejected", m_scenario.input.text);
        }
        if (got->to_string() != expected.parsed->id){
            return std::format("Expected id {}, got {}", expected.parsed->id, got->to_string());
        }
        if (got->segment_string() != expected.parsed->segment){
            return std::format("Expected segment {}, got {}", expected.parsed->segment, got->segment_string());
        }
        return true;
    }

private:
    const ParseScenario m_scenario;
};


//  Ordering: two ids, how the first compares to the second.

struct OrderInput{
    std::string left;
    std::string right;
};

struct OrderExpectation{
    std::strong_ordering order;
};

struct OrderScenario{
    std::string name;
    OrderInput input;
    OrderExpectation expectation;
};

std::string_view order_name(std::strong_ordering order){
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
        const std::optional<CheckpointId> left = CheckpointId::parse(m_scenario.input.left);
        const std::optional<CheckpointId> right = CheckpointId::parse(m_scenario.input.right);
        if (!left || !right){
            return std::format(
                "Bad checkpoint id \"{}\" or \"{}\" in the test input",
                m_scenario.input.left, m_scenario.input.right
            );
        }
        const std::strong_ordering got = *left <=> *right;
        if (got != m_scenario.expectation.order){
            return std::format(
                "Expected {} to be {} than {}, got {}",
                m_scenario.input.left,
                order_name(m_scenario.expectation.order),
                m_scenario.input.right,
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
    std::vector<std::string> passed;
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
        for (const std::string& text : m_scenario.input.passed){
            const std::optional<CheckpointId> next = CheckpointId::parse(text);
            if (!next){
                return std::format("Bad checkpoint id \"{}\" in the test input", text);
            }
            if (starts_segment(previous, *next)){
                got.emplace_back(next->to_string());
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


void add_parse_tests(UnitTestDatabase& database){
    const std::vector<ParseScenario> parse_cases{
        {.name = "segment and checkpoint", .input = {.text = "03.08"}, .expectation = {.parsed = ParsedId{.id = "03.08", .segment = "03"}}},
        {.name = "first checkpoint", .input = {.text = "00.00"}, .expectation = {.parsed = ParsedId{.id = "00.00", .segment = "00"}}},
        {.name = "largest id", .input = {.text = "99.99"}, .expectation = {.parsed = ParsedId{.id = "99.99", .segment = "99"}}},
        {.name = "letter suffix", .input = {.text = "12.02b"}, .expectation = {.parsed = ParsedId{.id = "12.02b", .segment = "12"}}},
        {.name = "first letter", .input = {.text = "00.12a"}, .expectation = {.parsed = ParsedId{.id = "00.12a", .segment = "00"}}},
        {.name = "last letter", .input = {.text = "03.08z"}, .expectation = {.parsed = ParsedId{.id = "03.08z", .segment = "03"}}},
        {.name = "uppercase letter", .input = {.text = "03.08B"}, .expectation = {}},
        {.name = "two letters", .input = {.text = "03.08bc"}, .expectation = {}},
        {.name = "digit after the letter", .input = {.text = "03.08b1"}, .expectation = {}},
        {.name = "third checkpoint digit", .input = {.text = "03.081"}, .expectation = {}},
        {.name = "punctuation suffix", .input = {.text = "03.08!"}, .expectation = {}},
        {.name = "letter in the segment", .input = {.text = "0b.08"}, .expectation = {}},
        {.name = "one-digit segment", .input = {.text = "3.08"}, .expectation = {}},
        {.name = "one-digit checkpoint", .input = {.text = "03.8"}, .expectation = {}},
        {.name = "three-digit segment", .input = {.text = "100.08"}, .expectation = {}},
        {.name = "three-digit checkpoint", .input = {.text = "03.100"}, .expectation = {}},
        {.name = "no dot", .input = {.text = "0308"}, .expectation = {}},
        {.name = "other separator", .input = {.text = "03-08"}, .expectation = {}},
        {.name = "extra part", .input = {.text = "03.08.01"}, .expectation = {}},
        {.name = "segment only", .input = {.text = "03"}, .expectation = {}},
        {.name = "sign", .input = {.text = "+3.08"}, .expectation = {}},
        {.name = "spaces", .input = {.text = " 3.08"}, .expectation = {}},
        {.name = "empty", .input = {.text = ""}, .expectation = {}},
    };
    for (const ParseScenario& scenario : parse_cases){
        database.add<Test_CheckpointIdParse>(scenario);
    }
}

void add_order_tests(UnitTestDatabase& database){
    const std::vector<OrderScenario> order_cases{
        {
            .name = "same id",
            .input = {.left = "03.08", .right = "03.08"},
            .expectation = {.order = std::strong_ordering::equal},
        },
        {
            .name = "later checkpoint in the same segment",
            .input = {.left = "03.09", .right = "03.08"},
            .expectation = {.order = std::strong_ordering::greater},
        },
        {
            .name = "segment wins over checkpoint",
            .input = {.left = "02.99", .right = "03.00"},
            .expectation = {.order = std::strong_ordering::less},
        },
        {
            .name = "no suffix before a suffix",
            .input = {.left = "12.02", .right = "12.02b"},
            .expectation = {.order = std::strong_ordering::less},
        },
        {
            .name = "suffixes in letter order",
            .input = {.left = "12.02b", .right = "12.02c"},
            .expectation = {.order = std::strong_ordering::less},
        },
        {
            .name = "suffix before the next checkpoint",
            .input = {.left = "12.02c", .right = "12.03"},
            .expectation = {.order = std::strong_ordering::less},
        },
        {
            .name = "same suffix",
            .input = {.left = "12.02b", .right = "12.02b"},
            .expectation = {.order = std::strong_ordering::equal},
        },
    };
    for (const OrderScenario& scenario : order_cases){
        database.add<Test_CheckpointIdOrder>(scenario);
    }
}

void add_segment_start_tests(UnitTestDatabase& database){
    const std::vector<SegmentStartScenario> segment_cases{
        {
            .name = "first checkpoint of a run starts its segment",
            .input = {.passed = {"03.08"}},
            .expectation = {.starts = {"03.08"}},
        },
        {
            .name = "checkpoints in one segment start it once",
            .input = {.passed = {"03.08", "03.09", "03.10"}},
            .expectation = {.starts = {"03.08"}},
        },
        {
            .name = "each new segment starts once",
            .input = {.passed = {"02.04", "02.05", "03.00", "03.01", "04.00"}},
            .expectation = {.starts = {"02.04", "03.00", "04.00"}},
        },
        {
            .name = "suffix does not start a segment",
            .input = {.passed = {"12.01", "12.02", "12.02b", "12.03"}},
            .expectation = {.starts = {"12.01"}},
        },
        {
            .name = "suffixed checkpoint can start a segment",
            .input = {.passed = {"00.12", "00.12b", "01.00"}},
            .expectation = {.starts = {"00.12", "01.00"}},
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


}  // namespace



void add_tests_CheckpointId(UnitTestDatabase& database){
    add_parse_tests(database);
    add_order_tests(database);
    add_segment_start_tests(database);
}



}  // namespace Pokemon
}  // namespace PokemonAutomation
