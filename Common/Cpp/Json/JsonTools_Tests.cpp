/*  JSON Tools Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "JsonValue.h"
#include "JsonTools_Tests.h"

namespace PokemonAutomation{
namespace{



struct DumpInput{
    std::string_view name;
    std::string_view text;
};
struct DumpExpectation{
    std::string_view dumped;
};
struct DumpCase{
    DumpInput input;
    DumpExpectation expectation;
};

//  Empty containers must keep their type through dump() and parse_json().
const DumpCase CASES[] = {
    {.input = {.name = "Empty object", .text = "{}"}, .expectation = {.dumped = "{}"}},
    {.input = {.name = "Nested empty object", .text = R"({"object":{}})"}, .expectation = {.dumped = R"({"object":{}})"}},
    {.input = {.name = "Nested empty array", .text = R"({"array":[]})"}, .expectation = {.dumped = R"({"array":[]})"}},
};



class Test_JsonTools_DumpRoundTrip : public UnitTest{
public:
    explicit Test_JsonTools_DumpRoundTrip(DumpCase test)
        : UnitTest(std::format("Common::JsonTools - {} round trip", test.input.name))
        , m_test(test)
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        const std::string dumped = parse_json(std::string(m_test.input.text)).dump(-1);
        if (dumped != m_test.expectation.dumped){
            return UnitTestResult(std::format("dump: expected {}, got {}", m_test.expectation.dumped, dumped));
        }
        const std::string again = parse_json(dumped).dump(-1);
        if (again != dumped){
            return UnitTestResult(std::format("dump after reparse: expected {}, got {}", dumped, again));
        }
        return true;
    }

private:
    DumpCase m_test;
};



} // namespace



void add_tests_JsonTools(UnitTestDatabase& database){
    for (const DumpCase& test : CASES){
        database.add<Test_JsonTools_DumpRoundTrip>(test);
    }
}



} // namespace PokemonAutomation
