/*  PokemonLZA AutoStory - Start load Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryStartLoad.h"
#include "PokemonLZA_AutoStoryStartLoad_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

namespace{


//  next_start_load_action(): the loads a run makes before its first
//  checkpoint, from the start checks each load gives.
struct StartLoadCase{
    std::string name;
    struct Input{
        StartFrom from;
        //  The start check after each load, in order; nothing: no check.
        std::vector<std::optional<bool>> checks;
    };
    Input input;
    struct Expectation{
        std::vector<StartLoadAction> actions;
    };
    Expectation expectation;
};

using enum StartLoadAction;

const StartLoadCase START_LOAD_CASES[] = {
    {.name = "the latest save at the start: start", .input = {.from = StartFrom::LATEST_SAVE, .checks = {true}}, .expectation = {.actions = {LOAD_LATEST, START}}},
    {.name = "no start check: start from the latest save", .input = {.from = StartFrom::LATEST_SAVE, .checks = {std::nullopt}}, .expectation = {.actions = {LOAD_LATEST, START}}},
    //  A catch in 03.08 autosaved in zone 1; the
    //  backup was the run's start, and the Report makes it the latest again.
    {.name = "the latest save past the start, the backup at it: save, then start", .input = {.from = StartFrom::LATEST_SAVE, .checks = {false, true}}, .expectation = {.actions = {LOAD_LATEST, LOAD_BACKUP, REPORT_THEN_START}}},
    {.name = "the latest save past the start, the backup with no check: start, no save", .input = {.from = StartFrom::LATEST_SAVE, .checks = {false, std::nullopt}}, .expectation = {.actions = {LOAD_LATEST, LOAD_BACKUP, START}}},
    {.name = "neither save at the start: stop", .input = {.from = StartFrom::LATEST_SAVE, .checks = {false, false}}, .expectation = {.actions = {LOAD_LATEST, LOAD_BACKUP, STOP}}},
    {.name = "backup first, at the start: start", .input = {.from = StartFrom::BACKUP_SAVE, .checks = {true}}, .expectation = {.actions = {LOAD_BACKUP, START}}},
    {.name = "backup first, not at the start: stop", .input = {.from = StartFrom::BACKUP_SAVE, .checks = {false}}, .expectation = {.actions = {LOAD_BACKUP, STOP}}},
    //  The game already at the run's start, the
    //  Pokemon Center counter; RELOAD_AT_START off keeps it.
    {.name = "running game at the start: start", .input = {.from = StartFrom::RUNNING_GAME, .checks = {true}}, .expectation = {.actions = {RESUME, START}}},
    {.name = "running game, no start check: start", .input = {.from = StartFrom::RUNNING_GAME, .checks = {std::nullopt}}, .expectation = {.actions = {RESUME, START}}},
    {.name = "running game not at the start: stop, no reload", .input = {.from = StartFrom::RUNNING_GAME, .checks = {false}}, .expectation = {.actions = {RESUME, STOP}}},
};

std::string_view action_name(StartLoadAction action){
    switch (action){
    case LOAD_LATEST:
        return "load latest";
    case LOAD_BACKUP:
        return "load backup";
    case RESUME:
        return "resume";
    case START:
        return "start";
    case REPORT_THEN_START:
        return "report, then start";
    case STOP:
        return "stop";
    }
    return "?";
}

std::string action_names(const std::vector<StartLoadAction>& actions){
    std::string out;
    for (StartLoadAction action : actions){
        out += std::format("{}{}", out.empty() ? "" : ", ", action_name(action));
    }
    return out;
}

class Test_StartLoad : public UnitTest{
public:
    explicit Test_StartLoad(const StartLoadCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryEngine - start load: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<StartLoad> loads;
        std::vector<StartLoadAction> actions;
        while (actions.size() < 5){
            const StartLoadAction action = next_start_load_action(m_case.input.from, loads);
            actions.push_back(action);
            if (action == START || action == REPORT_THEN_START || action == STOP){
                break;
            }
            if (loads.size() >= m_case.input.checks.size()){
                return std::format("Loaded more than the {} checks given: {}.", m_case.input.checks.size(), action_names(actions));
            }
            const StartSave save = action == LOAD_BACKUP ? StartSave::BACKUP : action == RESUME ? StartSave::RUNNING : StartSave::LATEST;
            loads.push_back({.save = save, .at_start = m_case.input.checks[loads.size()]});
        }
        if (actions != m_case.expectation.actions){
            return std::format("Expected [{}], got [{}].", action_names(m_case.expectation.actions), action_names(actions));
        }
        return true;
    }

private:
    const StartLoadCase& m_case;
};


//  checkpoint_start_sign(): what each checkpoint's start check looks for.
//  03.06 starts at the Pokemon Center counter; 03.08 has no check.
struct StartSignCase{
    std::string name;
    struct Input{
        std::string row;
    };
    Input input;
    struct Expectation{
        std::optional<ScreenEvent> event;
    };
    Expectation expectation;
};

const StartSignCase START_SIGN_CASES[] = {
    {.name = "03.06 starts at the counter", .input = {.row = "03.06"}, .expectation = {.event = ScreenEvent::INTERACT_PROMPT}},
    {.name = "03.08 has no start check", .input = {.row = "03.08"}, .expectation = {.event = std::nullopt}},
};

class Test_StartSign : public UnitTest{
public:
    explicit Test_StartSign(const StartSignCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryEngine - start sign: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<StartSign> sign = checkpoint_start_sign(m_case.input.row);
        const std::optional<ScreenEvent> event = sign ? std::optional<ScreenEvent>(sign->event) : std::nullopt;
        if (event != m_case.expectation.event){
            return std::format("Expected {}, got {}.",
                m_case.expectation.event ? screen_event_name(*m_case.expectation.event) : "no check",
                event ? screen_event_name(*event) : "no check"
            );
        }
        if (sign && sign->place.empty()){
            return "The sign names no place for the log.";
        }
        return true;
    }

private:
    const StartSignCase& m_case;
};


} // namespace


void add_tests_AutoStoryStartLoad(UnitTestDatabase& database){
    for (const StartLoadCase& item : START_LOAD_CASES){
        database.add<Test_StartLoad>(item);
    }
    for (const StartSignCase& item : START_SIGN_CASES){
        database.add<Test_StartSign>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
