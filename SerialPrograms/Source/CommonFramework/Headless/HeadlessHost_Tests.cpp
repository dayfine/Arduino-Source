/*  Headless Host Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Panels/PanelDescriptor.h"
#include "CommonFramework/Panels/PanelList.h"
#include "CommonFramework/Panels/PanelSession.h"
#include "CommonFramework/Panels/ProgramDescriptor.h"
#include "CommonFramework/ProgramSession.h"
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "HeadlessHost.h"
#include "HeadlessHost_Tests.h"

namespace PokemonAutomation{
namespace{

enum class FakeBehavior{
    SUCCEED,
    REPORT_ERROR,
    REFUSE_START,
};

class FakeProgramSession final : public PanelSession, public ProgramSession{
public:
    FakeProgramSession(const ProgramDescriptor& descriptor, FakeBehavior behavior)
        : PanelSession(descriptor)
        , ProgramSession(descriptor)
        , m_behavior(behavior)
    {}

    virtual ~FakeProgramSession() override{
        internal_stop_program();
        join_program_thread();
    }

protected:
    virtual std::unique_ptr<ProgramEnvironment> make_env(const ProgramInfo& program_info) override{
        return std::make_unique<ProgramEnvironment>(program_info, *this, nullptr, nullptr);
    }

    virtual void internal_run_program(ProgramEnvironment&) override{
        if (m_behavior == FakeBehavior::REPORT_ERROR){
            throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, "simulated failure");
        }
    }

    virtual std::string check_validity() const override{
        if (m_behavior == FakeBehavior::REFUSE_START){
            return "invalid configuration";
        }
        return "";
    }

private:
    FakeBehavior m_behavior;
};

class FakeProgramDescriptor final : public ProgramDescriptor{
public:
    FakeProgramDescriptor(std::string identifier, FakeBehavior behavior)
        : ProgramDescriptor(
            std::move(identifier),
            "Fake",
            "FakeProgram",
            /*doc_link=*/"",
            /*description=*/"",
            PanelQualityStanding::DEVELOPER,
            /*restore_defaults_button=*/false,
            /*required_resources=*/{}
        )
        , m_behavior(behavior)
    {}

    virtual std::unique_ptr<PanelSession> make_panel() const override{
        return std::make_unique<FakeProgramSession>(*this, m_behavior);
    }

private:
    FakeBehavior m_behavior;
};

class FakeSettingsDescriptor final : public PanelDescriptor{
public:
    explicit FakeSettingsDescriptor(std::string identifier)
        : PanelDescriptor(
            std::move(identifier),
            "Fake",
            "FakeSettings",
            /*doc_link=*/"",
            /*description=*/"",
            PanelQualityStanding::DEVELOPER,
            /*restore_defaults_button=*/false
        )
    {}

    virtual std::unique_ptr<PanelSession> make_panel() const override{
        return std::make_unique<PanelSession>(*this);
    }
};

class FakeListDescriptor final : public PanelListDescriptor{
public:
    FakeListDescriptor()
        : PanelListDescriptor("Fake")
    {}

protected:
    virtual std::vector<PanelEntry> make_panels() const override{
        std::vector<PanelEntry> ret;
        ret.emplace_back(std::make_unique<FakeProgramDescriptor>("Fake:Ok", FakeBehavior::SUCCEED));
        ret.emplace_back(std::make_unique<FakeProgramDescriptor>("Fake:Err", FakeBehavior::REPORT_ERROR));
        ret.emplace_back(std::make_unique<FakeProgramDescriptor>("Fake:Refused", FakeBehavior::REFUSE_START));
        ret.emplace_back(std::make_unique<FakeSettingsDescriptor>("Fake:Settings"));
        return ret;
    }
};

struct MergeInput{
    bool has_console_setup;
    bool is_object;
};

struct MergeExpectation{
    bool ok;
    std::string_view merged_a;
};

struct MergeCase{
    std::string_view name;
    MergeInput input;
    MergeExpectation expectation;
};

const MergeCase MERGE_CASES[] = {
    {.name = "merges override keys without ConsoleSetup",
     .input = {.has_console_setup = false, .is_object = true},
     .expectation = {.ok = true, .merged_a = "override"}},
    {.name = "rejects ConsoleSetup override",
     .input = {.has_console_setup = true, .is_object = true},
     .expectation = {.ok = false, .merged_a = ""}},
    {.name = "rejects non-object override",
     .input = {.has_console_setup = false, .is_object = false},
     .expectation = {.ok = false, .merged_a = ""}},
};

UnitTestResult check_merge_case(const MergeCase& test){
    JsonObject base;
    base["a"] = "base";
    base["b"] = "keep";

    JsonValue override_val;
    if (test.input.is_object){
        JsonObject obj;
        obj["a"] = "override";
        if (test.input.has_console_setup){
            obj["ConsoleSetup"] = JsonObject();
        }
        override_val = JsonValue(std::move(obj));
    }else{
        override_val = JsonValue(42);
    }

    std::string error;
    const std::optional<JsonValue> merged = merge_headless_options(JsonValue(std::move(base)), override_val, error);
    if (merged.has_value() != test.expectation.ok){
        return std::format("Expected merge ok={}; got {}.", test.expectation.ok, merged.has_value());
    }
    if (!merged){
        return true;
    }
    const JsonObject* obj = merged->to_object();
    const std::string* a = obj != nullptr ? obj->get_string("a") : nullptr;
    const std::string* b = obj != nullptr ? obj->get_string("b") : nullptr;
    if (a == nullptr || *a != test.expectation.merged_a || b == nullptr || *b != "keep"){
        return "Merged JSON values did not match expected keys.";
    }
    return true;
}

struct RunInput{
    std::string_view program_id;
};

struct RunExpectation{
    int exit_code;
};

struct RunCase{
    std::string_view name;
    RunInput input;
    RunExpectation expectation;
};

const RunCase RUN_CASES[] = {
    {.name = "clean program finishes 0", .input = {.program_id = "Fake:Ok"}, .expectation = {.exit_code = 0}},
    {.name = "program error finishes 1", .input = {.program_id = "Fake:Err"}, .expectation = {.exit_code = 1}},
    {.name = "unknown identifier finishes 4", .input = {.program_id = "Fake:Missing"}, .expectation = {.exit_code = 4}},
    {.name = "settings panel finishes 4", .input = {.program_id = "Fake:Settings"}, .expectation = {.exit_code = 4}},
    {.name = "refused start finishes 5", .input = {.program_id = "Fake:Refused"}, .expectation = {.exit_code = 5}},
};

UnitTestResult check_run_case(const RunCase& test){
    std::vector<std::unique_ptr<PanelListDescriptor>> lists;
    lists.push_back(std::make_unique<FakeListDescriptor>());
    const int code = run_headless_program(lists, {.program_id = std::string(test.input.program_id)});
    if (code != test.expectation.exit_code){
        return std::format("Expected exit code {}; got {}.", test.expectation.exit_code, code);
    }
    return true;
}

class MergeTest final : public UnitTest{
public:
    explicit MergeTest(const MergeCase& test)
        : UnitTest(std::format("HeadlessHost/Merge/{}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_merge_case(m_test);
    }
private:
    const MergeCase& m_test;
};

class RunTest final : public UnitTest{
public:
    explicit RunTest(const RunCase& test)
        : UnitTest(std::format("HeadlessHost/Run/{}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_run_case(m_test);
    }
private:
    const RunCase& m_test;
};

} // namespace

void add_tests_HeadlessHost(UnitTestDatabase& database){
    for (const MergeCase& test : MERGE_CASES){
        database.add<MergeTest>(test);
    }
    for (const RunCase& test : RUN_CASES){
        database.add<RunTest>(test);
    }
}

} // namespace PokemonAutomation
