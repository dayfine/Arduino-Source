/*  Story Verify Runs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Engine/Pokemon_StoryVerifyRuns.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryVerifyRuns_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

StoryCheckpointFactory scripted_factory(std::string id, std::vector<std::optional<StoryFailure>> results){
    size_t index = 0;
    return [id = std::move(id), results = std::move(results), index]() mutable -> StoryCheckpoint{
        const std::optional<StoryFailure> outcome = index < results.size() ? results[index] : std::nullopt;
        index++;
        StoryCheckpoint cp;
        cp.id = id;
        cp.title = id;
        StoryStep step;
        step.name = "fight";
        step.action = "battle";
        step.run = [outcome](StoryBackend&, RunState&) -> std::optional<StoryFailure>{
            return outcome;
        };
        cp.steps.push_back(std::move(step));
        return cp;
    };
}

StoryGame fake_game(size_t& saves, size_t& reloads){
    StoryGame game;
    game.save = [&saves](StoryBackend&, const StoryCheckpoint&) -> std::optional<StoryFailure>{
        saves++;
        return std::nullopt;
    };
    game.reload = [&reloads](StoryBackend&, const StoryCheckpoint& failed) -> std::optional<std::string>{
        reloads++;
        return failed.id;
    };
    return game;
}

struct VerifyModeCase{
    struct Input{
        std::string checkpoint_id;
        VerifyModePolicy policy;
        std::vector<std::optional<StoryFailure>> outcomes;
    };
    struct Expectation{
        bool verified = false;
        size_t runs = 0;
        size_t passes = 0;
        bool saved = false;
        size_t saves = 0;
        size_t reloads = 0;
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const VerifyModeCase VERIFY_MODE_CASES[] = {
    {
        .name = "two of three passes without saving",
        .input = {
            .checkpoint_id = "20.01",
            .policy = {.runs = 3, .passes_needed = 2, .save_mode = StorySaveMode::NONE},
            .outcomes = {std::nullopt, StoryFailure{.code = "blackout"}, std::nullopt},
        },
        .expectation = {.verified = true, .runs = 3, .passes = 2, .saved = false, .saves = 0, .reloads = 2},
    },
    {
        .name = "after-verification saves when every run passes",
        .input = {
            .checkpoint_id = "20.01",
            .policy = {.runs = 2, .passes_needed = 2, .save_mode = StorySaveMode::AFTER_VERIFICATION},
            .outcomes = {std::nullopt, std::nullopt},
        },
        .expectation = {.verified = true, .runs = 2, .passes = 2, .saved = true, .saves = 1, .reloads = 1},
    },
};

class Test_VerifyMode : public UnitTest{
public:
    explicit Test_VerifyMode(const VerifyModeCase& scenario)
        : UnitTest(std::format("Pokemon::StoryVerifyRuns - mode: {}", scenario.name))
        , m_scenario(scenario)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        FakeStoryBackend backend(FakeStoryScript{}, scope);
        size_t saves = 0;
        size_t reloads = 0;
        StoryGame game = fake_game(saves, reloads);
        RunState state;
        const VerifyModeReport report = verify_checkpoint(
            backend, m_scenario.input.checkpoint_id, m_scenario.input.policy,
            scripted_factory(m_scenario.input.checkpoint_id, m_scenario.input.outcomes),
            game, state
        );
        if (report.verified != m_scenario.expectation.verified
            || report.runs != m_scenario.expectation.runs
            || report.passes != m_scenario.expectation.passes
            || report.saved != m_scenario.expectation.saved
            || saves != m_scenario.expectation.saves
            || reloads != m_scenario.expectation.reloads
        ){
            return std::format(
                "report mismatch: verified={} runs={} passes={} saved={} saves={} reloads={}",
                report.verified, report.runs, report.passes, report.saved, saves, reloads
            );
        }
        return true;
    }

private:
    VerifyModeCase m_scenario;
};

struct VerifyRowCase{
    struct Input{
        std::string checkpoint_id = {};
        StoryVerify verify = {};
        std::vector<std::optional<StoryFailure>> outcomes = {};
        std::optional<StoryFailure> pass_check_failure = std::nullopt;
    };
    struct Expectation{
        bool passed = false;
        bool saved = false;
        size_t phase1_passes = 0;
        size_t attempts = 0;
        size_t reloads = 0;
        size_t saves = 0;
        std::string failure_code = {};
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const VerifyRowCase VERIFY_ROW_CASES[] = {
    {
        .name = "phase 1 passes then phase 2 saves on first pass",
        .input = {
            .checkpoint_id = "20.01",
            .verify = {.tries = 5, .min_pass = 2, .reset_budget = 3},
            .outcomes = {
                StoryFailure{.code = "blackout"},
                std::nullopt,
                std::nullopt,
                StoryFailure{.code = "blackout"},
                std::nullopt,
            },
        },
        .expectation = {.passed = true, .saved = true, .phase1_passes = 2, .attempts = 5, .reloads = 4, .saves = 1},
    },
    {
        .name = "stops early when phase 1 can no longer reach min_pass",
        .input = {
            .checkpoint_id = "20.02",
            .verify = {.tries = 4, .min_pass = 3, .reset_budget = 3},
            .outcomes = {StoryFailure{.code = "blackout"}, StoryFailure{.code = "blackout"}},
        },
        .expectation = {.passed = false, .saved = false, .phase1_passes = 0, .attempts = 2, .reloads = 1, .saves = 0, .failure_code = "verify_below_min_pass"},
    },
    {
        .name = "stops immediately on precondition_failed",
        .input = {
            .checkpoint_id = "20.03",
            .verify = {.tries = 5, .min_pass = 2, .reset_budget = 3},
            .outcomes = {StoryFailure{.code = "precondition_failed", .detail = "short of potions", .repeats = true}},
        },
        .expectation = {.passed = false, .saved = false, .phase1_passes = 0, .attempts = 1, .reloads = 0, .saves = 0, .failure_code = "precondition_failed"},
    },
    {
        .name = "stops immediately when pass check is unreadable",
        .input = {
            .checkpoint_id = "20.04",
            .verify = {.tries = 5, .min_pass = 2, .reset_budget = 3},
            .outcomes = {std::nullopt},
            .pass_check_failure = StoryFailure{.code = std::string(VERIFY_UNREAD), .detail = "summary unreadable", .repeats = true},
        },
        .expectation = {.passed = false, .saved = false, .phase1_passes = 0, .attempts = 1, .reloads = 0, .saves = 0, .failure_code = std::string(VERIFY_UNREAD)},
    },
    {
        .name = "skips phase 1 when tries is zero and saves on first phase 2 pass",
        .input = {
            .checkpoint_id = "20.01",
            .verify = {.tries = 0, .min_pass = 0, .reset_budget = 3},
            .outcomes = {StoryFailure{.code = "blackout"}, std::nullopt},
        },
        .expectation = {.passed = true, .saved = true, .phase1_passes = 0, .attempts = 2, .reloads = 1, .saves = 1},
    },
};

class Test_VerifyRow : public UnitTest{
public:
    explicit Test_VerifyRow(const VerifyRowCase& scenario)
        : UnitTest(std::format("Pokemon::StoryVerifyRuns - row: {}", scenario.name))
        , m_scenario(scenario)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        FakeStoryBackend backend(FakeStoryScript{}, scope);
        size_t saves = 0;
        size_t reloads = 0;
        StoryGame game = fake_game(saves, reloads);
        RunState state;
        VerifyPassPredicate predicate;
        if (m_scenario.input.pass_check_failure){
            const StoryFailure fail = *m_scenario.input.pass_check_failure;
            predicate = [fail](StoryBackend&, const StoryCheckpoint&, RunState&) -> std::optional<StoryFailure>{
                return fail;
            };
        }
        const VerifyRowReport report = run_verify_row(
            backend, m_scenario.input.checkpoint_id, m_scenario.input.verify,
            scripted_factory(m_scenario.input.checkpoint_id, m_scenario.input.outcomes),
            game, state, std::move(predicate)
        );
        const std::string got_code = report.failure ? report.failure->failure.code : "";
        if (report.passed != m_scenario.expectation.passed
            || report.saved != m_scenario.expectation.saved
            || report.phase1_passes != m_scenario.expectation.phase1_passes
            || report.attempts != m_scenario.expectation.attempts
            || report.reloads != m_scenario.expectation.reloads
            || saves != m_scenario.expectation.saves
            || got_code != m_scenario.expectation.failure_code
        ){
            return std::format(
                "row mismatch: passed={} saved={} p1={} attempts={} reloads={} saves={} code={}",
                report.passed, report.saved, report.phase1_passes, report.attempts, report.reloads, saves, got_code
            );
        }
        return true;
    }

private:
    VerifyRowCase m_scenario;
};

} // namespace


void add_tests_StoryVerifyRuns(UnitTestDatabase& database){
    for (const VerifyModeCase& scenario : VERIFY_MODE_CASES){
        database.add<Test_VerifyMode>(scenario);
    }
    for (const VerifyRowCase& scenario : VERIFY_ROW_CASES){
        database.add<Test_VerifyRow>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
