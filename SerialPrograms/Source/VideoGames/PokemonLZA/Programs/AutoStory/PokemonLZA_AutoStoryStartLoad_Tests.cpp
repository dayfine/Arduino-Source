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
    {.name = "the latest save past the start, the backup at it: start from the backup", .input = {.from = StartFrom::LATEST_SAVE, .checks = {false, true}}, .expectation = {.actions = {LOAD_LATEST, LOAD_BACKUP, START}}},
    {.name = "neither save at the start: stop", .input = {.from = StartFrom::LATEST_SAVE, .checks = {false, false}}, .expectation = {.actions = {LOAD_LATEST, LOAD_BACKUP, STOP}}},
    {.name = "backup first, at the start: start", .input = {.from = StartFrom::BACKUP_SAVE, .checks = {true}}, .expectation = {.actions = {LOAD_BACKUP, START}}},
    {.name = "backup first, not at the start: stop", .input = {.from = StartFrom::BACKUP_SAVE, .checks = {false}}, .expectation = {.actions = {LOAD_BACKUP, STOP}}},
    //  Console B, 2026-11-14: the game already at 03.07d's start, the
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
            if (action == START || action == STOP){
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


//  start_launch_request(): what the shared launch does for each start
//  save. Only RUNNING keeps the game: RELOAD_AT_START off.
struct StartLaunchCase{
    std::string name;
    struct Input{
        StartSave save;
    };
    Input input;
    struct Expectation{
        Pokemon::StoryLaunchRequest::Save save;
        Pokemon::StoryLaunchRequest::Running running;
    };
    Expectation expectation;
};

const StartLaunchCase START_LAUNCH_CASES[] = {
    {.name = "the latest save restarts", .input = {.save = StartSave::LATEST},
     .expectation = {.save = Pokemon::StoryLaunchRequest::Save::LATEST, .running = Pokemon::StoryLaunchRequest::Running::RESTART}},
    {.name = "the backup save restarts", .input = {.save = StartSave::BACKUP},
     .expectation = {.save = Pokemon::StoryLaunchRequest::Save::BACKUP, .running = Pokemon::StoryLaunchRequest::Running::RESTART}},
    {.name = "the running game is kept", .input = {.save = StartSave::RUNNING},
     .expectation = {.save = Pokemon::StoryLaunchRequest::Save::LATEST, .running = Pokemon::StoryLaunchRequest::Running::KEEP}},
};

class Test_StartLaunch : public UnitTest{
public:
    explicit Test_StartLaunch(const StartLaunchCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryEngine - start launch: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const Pokemon::StoryLaunchRequest request = start_launch_request(m_case.input.save);
        if (request.save != m_case.expectation.save || request.running != m_case.expectation.running){
            return "Unexpected launch request.";
        }
        return true;
    }

private:
    const StartLaunchCase& m_case;
};

} // namespace


void add_tests_AutoStoryStartLoad(UnitTestDatabase& database){
    for (const StartLoadCase& item : START_LOAD_CASES){
        database.add<Test_StartLoad>(item);
    }
    for (const StartLaunchCase& item : START_LAUNCH_CASES){
        database.add<Test_StartLaunch>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
