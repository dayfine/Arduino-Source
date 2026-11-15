/*  Lab Program Contract - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The option contract of "describe_program" for each option type, on a
 *  program's options built here: keys, types, defaults (not the current
 *  values), bounds and enum entries, nested groups, and static options left
 *  out; and the program's precheck list, or "unsupported" without one.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/FloatingPointOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonTools/Options/StringSelectOption.h"
#include "LabProgramContract.h"
#include "LabProgramContract_Tests.h"

namespace PokemonAutomation{

namespace{


//  A program's options with one of each type, every value changed from its
//  default so the contract shows it reads the default.
class SampleOptions{
public:
    SampleOptions()
        : options(LockMode::LOCK_WHILE_RUNNING)
        , NOTE("A note.")
        , ENABLED("Enabled", LockMode::LOCK_WHILE_RUNNING, true, false)
        , COUNT("Count", LockMode::LOCK_WHILE_RUNNING, 1, 10, 5, 7)
        , SPEED("Speed", LockMode::LOCK_WHILE_RUNNING, 0.5, 0.0, 2.0)
        , RATIO("Ratio", LockMode::LOCK_WHILE_RUNNING, 1.0)
        , NAME(false, "Name", LockMode::LOCK_WHILE_RUNNING, "Kosmos", "")
        , LANGUAGE(
            "Language",
            IntegerEnumDropdownDatabase{
                {0, "en", "English"},
                {1, "ja", "Japanese", false},
            },
            LockMode::LOCK_WHILE_RUNNING, 0
        )
        , CHECKPOINTS(create_string_select_database({"00", "01", "02"}))
        , START("Start", CHECKPOINTS, LockMode::LOCK_WHILE_RUNNING, "01")
        , NOTIFICATIONS("Notifications", LockMode::LOCK_WHILE_RUNNING, GroupOption::EnableMode::DEFAULT_ENABLED)
        , SCREENSHOT("Screenshot", LockMode::LOCK_WHILE_RUNNING, false)
    {
        options.add_option(NOTE, "");
        options.add_option(ENABLED, "ENABLED");
        options.add_option(COUNT, "COUNT");
        options.add_option(SPEED, "SPEED");
        options.add_option(RATIO, "RATIO");
        options.add_option(NAME, "NAME");
        options.add_option(LANGUAGE, "LANGUAGE");
        options.add_option(START, "START");
        NOTIFICATIONS.add_option(SCREENSHOT, "Screenshot");
        options.add_option(NOTIFICATIONS, "NOTIFICATIONS");
        SPEED.set(1.5);
        NAME.set("Red");
        LANGUAGE.set_value(1);
    }

    BatchOption options;
    StaticTextOption NOTE;
    BooleanCheckBoxOption ENABLED;
    SimpleIntegerOption<uint16_t> COUNT;
    FloatingPointOption SPEED;
    FloatingPointOption RATIO;
    StringOption NAME;
    IntegerEnumDropdownOption LANGUAGE;
    StringSelectDatabase CHECKPOINTS;
    StringSelectOption START;
    GroupOption NOTIFICATIONS;
    BooleanCheckBoxOption SCREENSHOT;
};


struct OptionInput{
    std::string key;
};
struct OptionExpectation{
    std::string json;   //  The option's entry, as compact JSON with sorted keys.
};
struct OptionCase{
    std::string name;
    OptionInput input;
    OptionExpectation expectation;
};

UnitTestResult check(const OptionCase& c){
    SampleOptions sample;
    const JsonArray contract = lab_option_contract(sample.options);
    for (const JsonValue& entry : contract){
        const JsonObject* object = entry.to_object();
        const std::string* key = object == nullptr ? nullptr : object->get_string(std::string("key"));
        if (key != nullptr && *key == c.input.key){
            const std::string text = entry.dump(-1);
            if (text != c.expectation.json){
                return std::format("expected\n{}\ngot\n{}", c.expectation.json, text);
            }
            return true;
        }
    }
    return std::format("no entry for {} in\n{}", c.input.key, JsonValue(contract.clone()).dump(-1));
}

class Test_LabOptionContract : public UnitTest{
public:
    explicit Test_LabOptionContract(OptionCase test_case)
        : UnitTest(std::format("CommonFramework::LabProgramContract - option - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }

private:
    OptionCase m_case;
};

const OptionCase OPTION_CASES[] = {
    {
        .name = "boolean reports its default, not its value",
        .input = {.key = "ENABLED"},
        .expectation = {.json = R"({"default":true,"key":"ENABLED","type":"boolean"})"},
    },
    {
        .name = "integer with bounds",
        .input = {.key = "COUNT"},
        .expectation = {.json = R"({"default":5,"key":"COUNT","max":10,"min":1,"type":"integer"})"},
    },
    {
        .name = "number with bounds",
        .input = {.key = "SPEED"},
        .expectation = {.json = R"({"default":0.5,"key":"SPEED","max":2.0,"min":0.0,"type":"number"})"},
    },
    {
        .name = "number without bounds",
        .input = {.key = "RATIO"},
        .expectation = {.json = R"({"default":1.0,"key":"RATIO","max":null,"min":null,"type":"number"})"},
    },
    {
        .name = "string",
        .input = {.key = "NAME"},
        .expectation = {.json = R"({"default":"Kosmos","key":"NAME","type":"string"})"},
    },
    {
        .name = "enum with a disabled entry",
        .input = {.key = "LANGUAGE"},
        .expectation = {.json = R"({"default":"en","entries":[{"enabled":true,"slug":"en"},{"enabled":false,"slug":"ja"}],"key":"LANGUAGE","type":"enum"})"},
    },
    {
        .name = "string select",
        .input = {.key = "START"},
        .expectation = {.json = R"({"default":"01","entries":[{"enabled":true,"slug":"00"},{"enabled":true,"slug":"01"},{"enabled":true,"slug":"02"}],"key":"START","type":"select"})"},
    },
    {
        .name = "toggleable group with its options",
        .input = {.key = "NOTIFICATIONS"},
        .expectation = {.json = R"({"key":"NOTIFICATIONS","options":[{"default":false,"key":"Screenshot","type":"boolean"}],"toggleable":true,"type":"group"})"},
    },
};


struct ProgramInput{
    std::string id;
    std::optional<std::vector<PrecheckSummary>> prechecks;
};
struct ProgramExpectation{
    size_t options;             //  Entries in "options": the static note is left out.
    std::string head;           //  The reply's other fields, as compact JSON.
};
struct ProgramCase{
    std::string name;
    ProgramInput input;
    ProgramExpectation expectation;
};

UnitTestResult check(const ProgramCase& c){
    SampleOptions sample;
    JsonObject contract = lab_program_contract(c.input.id, sample.options, c.input.prechecks);
    const JsonArray* options = contract.get_array(std::string("options"));
    if (options == nullptr || options->size() != c.expectation.options){
        return std::format("expected {} options in\n{}", c.expectation.options, JsonValue(std::move(contract)).dump(-1));
    }
    contract["options"] = JsonValue();
    const std::string head = JsonValue(std::move(contract)).dump(-1);
    if (head != c.expectation.head){
        return std::format("expected\n{}\ngot\n{}", c.expectation.head, head);
    }
    return true;
}

class Test_LabProgramContract : public UnitTest{
public:
    explicit Test_LabProgramContract(ProgramCase test_case)
        : UnitTest(std::format("CommonFramework::LabProgramContract - program - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }

private:
    ProgramCase m_case;
};

const ProgramCase PROGRAM_CASES[] = {
    {
        .name = "id, schema, prechecks unsupported, static options left out",
        .input = {.id = "PokemonFRLG:AutoStory"},
        .expectation = {
            .options = 8,
            .head = R"({"id":"PokemonFRLG:AutoStory","options":null,"prechecks":"unsupported","schema":1})",
        },
    },
    {
        .name = "a declared precheck list, in order",
        .input = {
            .id = "PokemonFRLG:GiftRng",
            .prechecks = std::vector<PrecheckSummary>{
                {.name = "black_border", .needs = "No black border around the game.", .skippable = false},
                {.name = "free_party_slots", .needs = "1 free party slot.", .skippable = true},
            },
        },
        .expectation = {
            .options = 8,
            .head = R"({"id":"PokemonFRLG:GiftRng","options":null,"prechecks":[{"name":"black_border","needs":"No black border around the game.","skippable":false},{"name":"free_party_slots","needs":"1 free party slot.","skippable":true}],"schema":1})",
        },
    },
    {
        .name = "an empty list declares no needs, not unsupported",
        .input = {
            .id = "PokemonFRLG:AutoStory",
            .prechecks = std::vector<PrecheckSummary>{},
        },
        .expectation = {
            .options = 8,
            .head = R"({"id":"PokemonFRLG:AutoStory","options":null,"prechecks":[],"schema":1})",
        },
    },
};


}  // namespace



void add_tests_LabProgramContract(UnitTestDatabase& database){
    for (const OptionCase& c : OPTION_CASES){
        database.add<Test_LabOptionContract>(c);
    }
    for (const ProgramCase& c : PROGRAM_CASES){
        database.add<Test_LabProgramContract>(c);
    }
}



}  // namespace PokemonAutomation
