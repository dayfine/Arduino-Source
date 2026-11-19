/*  Headless Host Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Panels/PanelDescriptor.h"
#include "CommonFramework/Panels/PanelList.h"
#include "CommonFramework/Panels/PanelSession.h"
#include "ComputerPrograms/ComputerProgram.h"
#include "HeadlessHost.h"
#include "HeadlessHost_Tests.h"

namespace PokemonAutomation{
namespace{

enum class FakeBehavior{SUCCEED, REPORT_ERROR, REFUSE_START};

class FakeInstance final : public ComputerProgramInstance{
public:
    explicit FakeInstance(FakeBehavior b) : m_b(b) {}
    virtual std::string check_validity() const override{
        return m_b == FakeBehavior::REFUSE_START ? "invalid configuration" : "";
    }
    virtual void program(ProgramEnvironment&, CancellableScope&) override{
        if (m_b == FakeBehavior::REPORT_ERROR){
            throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, "simulated failure");
        }
    }
private:
    FakeBehavior m_b;
};

class FakeProgramDescriptor final : public ComputerProgramDescriptor{
public:
    FakeProgramDescriptor(std::string id, FakeBehavior b)
        : ComputerProgramDescriptor(std::move(id), "Fake", "FakeProgram", /*doc_link=*/"", /*description=*/"", PanelQualityStanding::DEVELOPER)
        , m_b(b)
    {}
    virtual std::unique_ptr<ComputerProgramInstance> make_instance() const override{
        return std::make_unique<FakeInstance>(m_b);
    }
private:
    FakeBehavior m_b;
};

class FakeSettingsDescriptor final : public PanelDescriptor{
public:
    explicit FakeSettingsDescriptor(std::string id)
        : PanelDescriptor(std::move(id), "Fake", "FakeSettings", /*doc_link=*/"", /*description=*/"", PanelQualityStanding::DEVELOPER, /*restore_defaults_button=*/false)
    {}
    virtual std::unique_ptr<PanelSession> make_panel() const override{
        return std::make_unique<PanelSession>(*this);
    }
};

class FakeListDescriptor final : public PanelListDescriptor{
public:
    FakeListDescriptor() : PanelListDescriptor("Fake") {}
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

struct RunInput{
    std::string_view program_id;
    std::optional<std::string_view> options_json = std::nullopt;
};

struct RunExpectation{
    HeadlessExit exit_code;
};

struct RunCase{
    std::string_view name;
    RunInput input;
    RunExpectation expectation;
};

const RunCase RUN_CASES[] = {
    {.name = "clean program finishes 0", .input = {.program_id = "Fake:Ok"}, .expectation = {.exit_code = HeadlessExit::OK}},
    {.name = "program error finishes 1", .input = {.program_id = "Fake:Err"}, .expectation = {.exit_code = HeadlessExit::PROGRAM_ERROR}},
    {.name = "unknown identifier finishes 4", .input = {.program_id = "Fake:Missing"}, .expectation = {.exit_code = HeadlessExit::BAD_REQUEST}},
    {.name = "settings panel finishes 4", .input = {.program_id = "Fake:Settings"}, .expectation = {.exit_code = HeadlessExit::BAD_REQUEST}},
    {.name = "refused start finishes 5", .input = {.program_id = "Fake:Refused"}, .expectation = {.exit_code = HeadlessExit::DEVICE}},
    {.name = "ConsoleSetup override finishes 4", .input = {.program_id = "Fake:Ok", .options_json = "{\"ConsoleSetup\": {}}"}, .expectation = {.exit_code = HeadlessExit::BAD_REQUEST}},
    {.name = "non-object override finishes 4", .input = {.program_id = "Fake:Ok", .options_json = "42"}, .expectation = {.exit_code = HeadlessExit::BAD_REQUEST}},
};

UnitTestResult check_run_case(const RunCase& test){
    std::vector<std::unique_ptr<PanelListDescriptor>> lists;
    lists.push_back(std::make_unique<FakeListDescriptor>());
    HeadlessRunOptions options{
        .program_id = std::string(test.input.program_id),
        .base_panel_json = JsonValue(),
        .handle_signals = false,
    };
    if (test.input.options_json){
        options.options_override = parse_json(std::string(*test.input.options_json));
    }
    const HeadlessExit code = run_headless_program(lists, options);
    if (code != test.expectation.exit_code){
        return std::format("Expected exit code {}; got {}.", static_cast<int>(test.expectation.exit_code), static_cast<int>(code));
    }
    return true;
}

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
    for (const RunCase& test : RUN_CASES){
        database.add<RunTest>(test);
    }
}

} // namespace PokemonAutomation
