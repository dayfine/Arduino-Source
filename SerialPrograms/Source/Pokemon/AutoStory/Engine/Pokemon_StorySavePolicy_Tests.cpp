/*  Story Save Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StorySavePolicy.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "Pokemon_StorySavePolicy_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

class SavePolicyTest : public UnitTest{
public:
    SavePolicyTest(std::string name, std::function<UnitTestResult(Logger&, CancellableScope&)> fn)
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

StoryCheckpoint make_passing_checkpoint(std::string id){
    StoryCheckpoint cp;
    cp.id = std::move(id);
    cp.title = cp.id;
    StoryStep step;
    step.name = "step1";
    step.action = "noop";
    step.run = [](StoryBackend&, RunState&) -> std::optional<StoryFailure>{
        return std::nullopt;
    };
    cp.steps.push_back(std::move(step));
    return cp;
}

UnitTestResult test_status_names_and_modes(Logger&, CancellableScope&){
    PA_STORY_EXPECT(CHECKPOINT_TESTING_STATUS_NAMES().get_enum("testing") == CheckpointTestingStatus::TESTING);
    PA_STORY_EXPECT(CHECKPOINT_TESTING_STATUS_NAMES().get_enum("verification") == CheckpointTestingStatus::VERIFICATION);
    PA_STORY_EXPECT(CHECKPOINT_TESTING_STATUS_NAMES().get_enum("reproduction") == CheckpointTestingStatus::REPRODUCTION);
    PA_STORY_EXPECT(CHECKPOINT_TESTING_STATUS_NAMES().get_string(CheckpointTestingStatus::VERIFICATION) == "verification");

    bool threw = false;
    try{
        (void)CHECKPOINT_TESTING_STATUS_NAMES().get_enum("unknown");
    }catch (const ParseException&){
        threw = true;
    }
    PA_STORY_EXPECT(threw);

    PA_STORY_EXPECT(effective_save_mode(StorySaveMode::PER_CHECKPOINT, CheckpointTestingStatus::TESTING) == StorySaveMode::NONE);
    PA_STORY_EXPECT(effective_save_mode(StorySaveMode::PER_CHECKPOINT, CheckpointTestingStatus::VERIFICATION) == StorySaveMode::AFTER_VERIFICATION);
    PA_STORY_EXPECT(effective_save_mode(StorySaveMode::AFTER_VERIFICATION, CheckpointTestingStatus::REPRODUCTION) == StorySaveMode::AFTER_VERIFICATION);
    PA_STORY_EXPECT(effective_save_mode(StorySaveMode::PER_CHECKPOINT, CheckpointTestingStatus::REPRODUCTION) == StorySaveMode::PER_CHECKPOINT);
    return true;
}

UnitTestResult test_phase_gated_respects_phase_and_run_end(Logger&, CancellableScope& scope){
    FakeStoryBackend backend(FakeStoryScript{}, scope);
    std::vector<std::string> saved;
    std::vector<StoryCheckpoint> cps;
    cps.push_back(make_passing_checkpoint("01.01"));
    cps.push_back(make_passing_checkpoint("01.02"));
    cps.push_back(make_passing_checkpoint("01.03"));

    PhaseGateConfig config;
    config.requested_mode = StorySaveMode::PER_CHECKPOINT;
    config.last_checkpoint_id = "01.03";
    config.status_by_id = {
        {"01.01", CheckpointTestingStatus::TESTING},
        {"01.02", CheckpointTestingStatus::REPRODUCTION},
        {"01.03", CheckpointTestingStatus::VERIFICATION},
    };

    StoryGame game;
    game.save = phase_gated(
        [&saved](StoryBackend&, const StoryCheckpoint& passed) -> std::optional<StoryFailure>{
            saved.push_back(passed.id);
            return std::nullopt;
        },
        std::move(config)
    );

    RunState state;
    const std::optional<CheckpointFailure> failure = run_checkpoints(backend, cps, game, state);
    PA_STORY_EXPECT(!failure);
    //  01.01: TESTING -> NONE (skipped)
    //  01.02: REPRODUCTION -> PER_CHECKPOINT (saved)
    //  01.03: VERIFICATION + RUN_END -> AFTER_VERIFICATION allows RUN_END (saved)
    PA_STORY_EXPECT(saved.size() == 2);
    PA_STORY_EXPECT(saved[0] == "01.02");
    PA_STORY_EXPECT(saved[1] == "01.03");
    return true;
}

UnitTestResult test_phase_gated_ignore_phase_uses_requested_mode(Logger&, CancellableScope& scope){
    FakeStoryBackend backend(FakeStoryScript{}, scope);
    std::vector<std::string> saved;
    std::vector<StoryCheckpoint> cps;
    cps.push_back(make_passing_checkpoint("01.01"));
    cps.push_back(make_passing_checkpoint("01.02"));

    PhaseGateConfig config;
    config.requested_mode = StorySaveMode::AFTER_VERIFICATION;
    config.last_checkpoint_id = "01.02";
    config.status_by_id = {
        {"01.01", CheckpointTestingStatus::TESTING},
        {"01.02", CheckpointTestingStatus::TESTING},
    };
    config.ignore_phase = true;

    StoryGame game;
    game.save = phase_gated(
        [&saved](StoryBackend&, const StoryCheckpoint& passed) -> std::optional<StoryFailure>{
            saved.push_back(passed.id);
            return std::nullopt;
        },
        std::move(config)
    );

    RunState state;
    PA_STORY_EXPECT(!run_checkpoints(backend, cps, game, state));
    PA_STORY_EXPECT(saved.size() == 1);
    PA_STORY_EXPECT(saved[0] == "01.02");
    return true;
}

#undef PA_STORY_EXPECT

} // namespace


void add_tests_StorySavePolicy(UnitTestDatabase& database){
    database.add<SavePolicyTest>("AutoStory/StorySavePolicy/StatusNamesAndModes", test_status_names_and_modes);
    database.add<SavePolicyTest>("AutoStory/StorySavePolicy/PhaseGatedRespectsPhaseAndRunEnd", test_phase_gated_respects_phase_and_run_end);
    database.add<SavePolicyTest>("AutoStory/StorySavePolicy/PhaseGatedIgnorePhase", test_phase_gated_ignore_phase_uses_requested_mode);
}



} // namespace Pokemon
} // namespace PokemonAutomation
