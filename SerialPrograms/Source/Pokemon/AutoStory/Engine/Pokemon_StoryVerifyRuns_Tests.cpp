/*  Story Verify Runs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Engine/Pokemon_StoryVerifyRuns.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryVerifyRuns_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

class VerifyRunsTest : public UnitTest{
public:
    VerifyRunsTest(std::string name, std::function<UnitTestResult(Logger&, CancellableScope&)> fn)
        : UnitTest(std::move(name))
        , m_fn(std::move(fn))
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        return m_fn(logger, scope);
    }

private:
    std::function<UnitTestResult(Logger&, CancellableScope&)> m_fn;
};

#define PA_STORY_EXPECT(cond) do{ if (!(cond)){ return "assertion failed: " #cond; } }while (false)

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

UnitTestResult test_verify_checkpoint_two_of_three_no_save(Logger&, CancellableScope& scope){
    FakeStoryBackend backend(FakeStoryScript{}, scope);
    size_t saves = 0;
    size_t reloads = 0;
    StoryGame game = fake_game(saves, reloads);
    RunState state;

    const VerifyModeReport report = verify_checkpoint(
        backend, "20.01",
        VerifyModePolicy{.runs = 3, .passes_needed = 2, .save_mode = StorySaveMode::NONE},
        scripted_factory("20.01", {std::nullopt, StoryFailure{.code = "blackout"}, std::nullopt}),
        game, state
    );
    PA_STORY_EXPECT(report.verified);
    PA_STORY_EXPECT(report.runs == 3);
    PA_STORY_EXPECT(report.passes == 2);
    PA_STORY_EXPECT(!report.saved);
    PA_STORY_EXPECT(saves == 0);
    PA_STORY_EXPECT(reloads == 2);
    return true;
}

UnitTestResult test_verify_checkpoint_after_verification_saves_only_when_all_pass(Logger&, CancellableScope& scope){
    FakeStoryBackend backend(FakeStoryScript{}, scope);
    size_t saves = 0;
    size_t reloads = 0;
    StoryGame game = fake_game(saves, reloads);
    RunState state;

    const VerifyModeReport report = verify_checkpoint(
        backend, "20.01",
        VerifyModePolicy{.runs = 2, .passes_needed = 2, .save_mode = StorySaveMode::AFTER_VERIFICATION},
        scripted_factory("20.01", {std::nullopt, std::nullopt}),
        game, state
    );
    PA_STORY_EXPECT(report.verified);
    PA_STORY_EXPECT(report.saved);
    PA_STORY_EXPECT(saves == 1);
    PA_STORY_EXPECT(reloads == 1);
    return true;
}

UnitTestResult test_verify_row_phase1_and_phase2_save(Logger&, CancellableScope& scope){
    FakeStoryBackend backend(FakeStoryScript{}, scope);
    size_t saves = 0;
    size_t reloads = 0;
    StoryGame game = fake_game(saves, reloads);
    RunState state;

    //  Phase 1: fail, pass, pass (reaches min_pass = 2 in 3 tries, 0 saves)
    //  Phase 2: fail, pass (saves on try 2)
    const VerifyRowReport report = run_verify_row(
        backend, "20.01",
        StoryVerify{.tries = 5, .min_pass = 2, .reset_budget = 3},
        scripted_factory("20.01", {
            StoryFailure{.code = "blackout"},
            std::nullopt,
            std::nullopt,
            StoryFailure{.code = "blackout"},
            std::nullopt,
        }),
        game, state
    );
    PA_STORY_EXPECT(report.passed);
    PA_STORY_EXPECT(report.saved);
    PA_STORY_EXPECT(report.phase1_passes == 2);
    PA_STORY_EXPECT(report.attempts == 5);
    PA_STORY_EXPECT(report.reloads == 4);
    PA_STORY_EXPECT(saves == 1);
    PA_STORY_EXPECT(reloads == 4);
    return true;
}

UnitTestResult test_verify_row_stops_early_below_min_pass(Logger&, CancellableScope& scope){
    FakeStoryBackend backend(FakeStoryScript{}, scope);
    size_t saves = 0;
    size_t reloads = 0;
    StoryGame game = fake_game(saves, reloads);
    RunState state;

    //  tries=4, min_pass=3: after 2 failures, only 2 tries left -> impossible!
    const VerifyRowReport report = run_verify_row(
        backend, "20.02",
        StoryVerify{.tries = 4, .min_pass = 3, .reset_budget = 3},
        scripted_factory("20.02", {
            StoryFailure{.code = "blackout"},
            StoryFailure{.code = "blackout"},
        }),
        game, state
    );
    PA_STORY_EXPECT(!report.passed);
    PA_STORY_EXPECT(!report.saved);
    PA_STORY_EXPECT(report.attempts == 2);
    PA_STORY_EXPECT(report.failure && report.failure->failure.code == "verify_below_min_pass");
    PA_STORY_EXPECT(saves == 0);
    return true;
}

UnitTestResult test_verify_row_stops_on_precondition_and_unread(Logger&, CancellableScope& scope){
    FakeStoryBackend backend(FakeStoryScript{}, scope);
    size_t saves = 0;
    size_t reloads = 0;
    StoryGame game = fake_game(saves, reloads);
    RunState state;

    const VerifyRowReport precond = run_verify_row(
        backend, "20.03",
        StoryVerify{.tries = 5, .min_pass = 2, .reset_budget = 3},
        scripted_factory("20.03", {
            StoryFailure{.code = "precondition_failed", .detail = "short of potions", .repeats = true},
        }),
        game, state
    );
    PA_STORY_EXPECT(!precond.passed);
    PA_STORY_EXPECT(precond.attempts == 1);
    PA_STORY_EXPECT(precond.failure && precond.failure->failure.code == "precondition_failed");
    PA_STORY_EXPECT(saves == 0);

    const VerifyRowReport unread = run_verify_row(
        backend, "20.04",
        StoryVerify{.tries = 5, .min_pass = 2, .reset_budget = 3},
        scripted_factory("20.04", {std::nullopt}),
        game, state,
        [](StoryBackend&, const StoryCheckpoint&, RunState&) -> std::optional<StoryFailure>{
            return StoryFailure{.code = std::string(VERIFY_UNREAD), .detail = "summary unreadable", .repeats = true};
        }
    );
    PA_STORY_EXPECT(!unread.passed);
    PA_STORY_EXPECT(unread.attempts == 1);
    PA_STORY_EXPECT(unread.failure && unread.failure->failure.code == VERIFY_UNREAD);
    PA_STORY_EXPECT(saves == 0);
    return true;
}

#undef PA_STORY_EXPECT

} // namespace


void add_tests_StoryVerifyRuns(UnitTestDatabase& database){
    database.add<VerifyRunsTest>("AutoStory/StoryVerifyRuns/VerifyCheckpointTwoOfThree", test_verify_checkpoint_two_of_three_no_save);
    database.add<VerifyRunsTest>("AutoStory/StoryVerifyRuns/VerifyCheckpointAfterVerificationSaves", test_verify_checkpoint_after_verification_saves_only_when_all_pass);
    database.add<VerifyRunsTest>("AutoStory/StoryVerifyRuns/VerifyRowPhase1AndPhase2Save", test_verify_row_phase1_and_phase2_save);
    database.add<VerifyRunsTest>("AutoStory/StoryVerifyRuns/VerifyRowStopsEarlyBelowMinPass", test_verify_row_stops_early_below_min_pass);
    database.add<VerifyRunsTest>("AutoStory/StoryVerifyRuns/VerifyRowStopsOnPreconditionAndUnread", test_verify_row_stops_on_precondition_and_unread);
}



} // namespace Pokemon
} // namespace PokemonAutomation
