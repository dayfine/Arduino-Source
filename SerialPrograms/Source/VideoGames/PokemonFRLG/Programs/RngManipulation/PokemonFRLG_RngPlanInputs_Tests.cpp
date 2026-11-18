/*  RNG Plan Inputs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_RngPlanInputs.h"
#include "PokemonFRLG_RngPlanInputs_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


namespace{

struct RngPlanInputsInput{
    std::string name;
    UnitTestResult (*body)(Logger&);
};

struct RngPlanInputsExpectation{
    std::string_view description;
};

struct RngPlanInputsTestCase{
    RngPlanInputsInput input;
    RngPlanInputsExpectation expectation;
};

class Test_RngPlanInputs : public UnitTest{
public:
    explicit Test_RngPlanInputs(RngPlanInputsTestCase scenario)
        : UnitTest("PokemonFRLG::RngPlanInputs - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_scenario.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    RngPlanInputsTestCase m_scenario;
};


UnitTestResult test_parse_trainer_id(Logger&){
    struct Case{
        std::string text;
        std::string expected;
    };
    const Case cases[] = {
        {.text = "64694", .expected = "64694"},
        {.text = " 0 ", .expected = "0"},
        {.text = "65535", .expected = "65535"},
        {.text = "65536", .expected = "none"},
        {.text = "", .expected = "none"},
        {.text = "-1", .expected = "none"},
        {.text = "12a", .expected = "none"},
    };
    for (const Case& item : cases){
        const std::optional<uint16_t> sid = parse_trainer_id(item.text);
        const std::string got = sid ? std::to_string(*sid) : "none";
        if (got != item.expected){
            return std::format("SID \"{}\": expected {}, got {}.", item.text, item.expected, got);
        }
    }
    return true;
}

UnitTestResult test_parse_ivs(Logger&){
    struct Case{
        std::string text;
        std::string expected;
    };
    const Case cases[] = {
        {.text = "31/14/7/29/1/27", .expected = "31/14/7/29/1/27"},
        {.text = " 0 / 0/0/0/0/31 ", .expected = "0/0/0/0/0/31"},
        {.text = "31/0/0", .expected = "none"},
        {.text = "32/0/0/0/0/0", .expected = "none"},
        {.text = "a/0/0/0/0/0", .expected = "none"},
        {.text = "-1/0/0/0/0/0", .expected = "none"},
        {.text = "", .expected = "none"},
    };
    for (const Case& item : cases){
        const std::optional<AdvIVs> ivs = parse_ivs(item.text);
        const std::string got = ivs ? ivs_to_string(*ivs) : "none";
        if (got != item.expected){
            return std::format("IVs \"{}\": expected {}, got {}.", item.text, item.expected, got);
        }
    }
    return true;
}

UnitTestResult test_parse_natures(Logger&){
    struct Case{
        std::string text;
        std::string expected;
    };
    const Case cases[] = {
        {.text = "", .expected = "any"},
        {.text = "  ", .expected = "any"},
        {.text = "Modest", .expected = "Modest"},
        {.text = "Modest, Timid", .expected = "Modest Timid"},
        {.text = "Modset", .expected = "none"},
        {.text = "Modest,", .expected = "none"},
    };
    for (const Case& item : cases){
        const std::optional<std::vector<AdvNature>> natures = parse_natures(item.text);
        std::string got;
        if (!natures){
            got = "none";
        }else if (natures->empty()){
            got = "any";
        }else{
            for (AdvNature nature : *natures){
                got += (got.empty() ? "" : " ") + nature_to_string(nature);
            }
        }
        if (got != item.expected){
            return std::format("Natures \"{}\": expected {}, got {}.", item.text, item.expected, got);
        }
    }
    return true;
}

const RngPlanInputsTestCase RNG_PLAN_INPUTS_CASES[] = {
    {
        .input = {.name = "TID and SID text", .body = test_parse_trainer_id},
        .expectation = {.description = "parses TID and SID text formats"},
    },
    {
        .input = {.name = "IVs text", .body = test_parse_ivs},
        .expectation = {.description = "parses six individual IV integer values"},
    },
    {
        .input = {.name = "natures text", .body = test_parse_natures},
        .expectation = {.description = "parses comma-separated nature names"},
    },
};

} // namespace


void add_tests_RngPlanInputs(UnitTestDatabase& database){
    for (const auto& scenario : RNG_PLAN_INPUTS_CASES){
        database.add<Test_RngPlanInputs>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
