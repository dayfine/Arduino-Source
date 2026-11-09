/*  JSON Tools Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "JsonObject.h"
#include "JsonValue.h"
#include "JsonTools_Tests.h"

namespace PokemonAutomation{
namespace{



class Test_JsonTools_EmptyObjectRoundTrip : public UnitTest{
public:
    Test_JsonTools_EmptyObjectRoundTrip()
        : UnitTest("Common::JsonTools - Empty object round trip")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        const std::string empty = JsonValue(JsonObject()).dump(-1);
        if (empty != "{}"){
            return UnitTestResult("dump of an empty object: expected {}, got " + empty);
        }

        const std::string text = parse_json(R"({"array":[],"object":{}})").dump(-1);
        const JsonValue reparsed = parse_json(text);
        const JsonObject* root = reparsed.to_object();
        if (root == nullptr){
            return UnitTestResult("reparsed document: expected an object, got " + text);
        }
        const JsonValue* object = root->get_value("object");
        if (object == nullptr || !object->is_object()){
            return UnitTestResult("nested empty object: expected an object, got " + text);
        }
        const JsonValue* array = root->get_value("array");
        if (array == nullptr || !array->is_array()){
            return UnitTestResult("nested empty array: expected an array, got " + text);
        }
        return true;
    }
};



} // anonymous namespace



void add_tests_JsonTools(UnitTestDatabase& database){
    database.add<Test_JsonTools_EmptyObjectRoundTrip>();
}



}
