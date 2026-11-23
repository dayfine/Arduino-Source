/*  Story Save Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StorySavePolicy.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "Pokemon_StorySavePolicy_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

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

struct EffectiveModeCase{
    struct Input{
        StorySaveMode requested = StorySaveMode::NONE;
        CheckpointTestingStatus status = CheckpointTestingStatus::TESTING;
        std::string_view status_text = "";
    };
    struct Expectation{
        StorySaveMode effective = StorySaveMode::NONE;
        bool parse_throws = false;
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const EffectiveModeCase EFFECTIVE_MODE_CASES[] = {
    {
        .name = "testing status disables saves",
        .input = {.requested = StorySaveMode::PER_CHECKPOINT, .status = CheckpointTestingStatus::TESTING, .status_text = "testing"},
        .expectation = {.effective = StorySaveMode::NONE},
    },
    {
        .name = "verification status clamps per-checkpoint to after-verification",
        .input = {.requested = StorySaveMode::PER_CHECKPOINT, .status = CheckpointTestingStatus::VERIFICATION, .status_text = "verification"},
        .expectation = {.effective = StorySaveMode::AFTER_VERIFICATION},
    },
    {
        .name = "reproduction status preserves after-verification",
        .input = {.requested = StorySaveMode::AFTER_VERIFICATION, .status = CheckpointTestingStatus::REPRODUCTION, .status_text = "reproduction"},
        .expectation = {.effective = StorySaveMode::AFTER_VERIFICATION},
    },
    {
        .name = "reproduction status allows per-checkpoint",
        .input = {.requested = StorySaveMode::PER_CHECKPOINT, .status = CheckpointTestingStatus::REPRODUCTION, .status_text = "reproduction"},
        .expectation = {.effective = StorySaveMode::PER_CHECKPOINT},
    },
    {
        .name = "unknown status name throws ParseException",
        .input = {.status_text = "unknown"},
        .expectation = {.parse_throws = true},
    },
};

class Test_EffectiveSaveMode : public UnitTest{
public:
    explicit Test_EffectiveSaveMode(const EffectiveModeCase& scenario)
        : UnitTest(std::format("Pokemon::StorySavePolicy - mode: {}", scenario.name))
        , m_scenario(scenario)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (m_scenario.expectation.parse_throws){
            try{
                (void)CHECKPOINT_TESTING_STATUS_NAMES().get_enum(std::string(m_scenario.input.status_text));
                return "expected ParseException for unknown status";
            }catch (const ParseException&){
                return true;
            }
        }
        const CheckpointTestingStatus parsed = CHECKPOINT_TESTING_STATUS_NAMES().get_enum(std::string(m_scenario.input.status_text));
        if (parsed != m_scenario.input.status){
            return "status name did not round-trip to expected enum";
        }
        if (CHECKPOINT_TESTING_STATUS_NAMES().get_string(m_scenario.input.status) != m_scenario.input.status_text){
            return "status enum did not round-trip to expected string";
        }
        const StorySaveMode got = effective_save_mode(m_scenario.input.requested, m_scenario.input.status);
        if (got != m_scenario.expectation.effective){
            return "effective_save_mode() returned unexpected mode";
        }
        return true;
    }

private:
    EffectiveModeCase m_scenario;
};

struct PhaseGateCase{
    struct Input{
        StorySaveMode requested_mode = StorySaveMode::NONE;
        bool ignore_phase = false;
        std::vector<std::pair<std::string, CheckpointTestingStatus>> checkpoints;
    };
    struct Expectation{
        std::vector<std::string> saved;
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const PhaseGateCase PHASE_GATE_CASES[] = {
    {
        .name = "respects testing, reproduction, and run-end verification phases",
        .input = {
            .requested_mode = StorySaveMode::PER_CHECKPOINT,
            .ignore_phase = false,
            .checkpoints = {
                {"01.01", CheckpointTestingStatus::TESTING},
                {"01.02", CheckpointTestingStatus::REPRODUCTION},
                {"01.03", CheckpointTestingStatus::VERIFICATION},
            },
        },
        .expectation = {.saved = {"01.02", "01.03"}},
    },
    {
        .name = "ignore_phase uses requested mode across testing checkpoints",
        .input = {
            .requested_mode = StorySaveMode::AFTER_VERIFICATION,
            .ignore_phase = true,
            .checkpoints = {
                {"01.01", CheckpointTestingStatus::TESTING},
                {"01.02", CheckpointTestingStatus::TESTING},
            },
        },
        .expectation = {.saved = {"01.02"}},
    },
};

class Test_PhaseGated : public UnitTest{
public:
    explicit Test_PhaseGated(const PhaseGateCase& scenario)
        : UnitTest(std::format("Pokemon::StorySavePolicy - phase gate: {}", scenario.name))
        , m_scenario(scenario)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        FakeStoryBackend backend(FakeStoryScript{}, scope);
        std::vector<std::string> saved;
        std::vector<StoryCheckpoint> cps;
        PhaseGateConfig config;
        config.requested_mode = m_scenario.input.requested_mode;
        config.ignore_phase = m_scenario.input.ignore_phase;
        for (const auto& [id, status] : m_scenario.input.checkpoints){
            cps.push_back(make_passing_checkpoint(id));
            config.status_by_id.emplace(id, status);
            config.last_checkpoint_id = id;
        }
        StoryGame game;
        game.save = phase_gated(
            [&saved](StoryBackend&, const StoryCheckpoint& passed) -> std::optional<StoryFailure>{
                saved.push_back(passed.id);
                return std::nullopt;
            },
            std::move(config)
        );
        RunState state;
        if (run_checkpoints(backend, cps, game, state)){
            return "run_checkpoints() failed unexpectedly";
        }
        if (saved != m_scenario.expectation.saved){
            return "saved checkpoint ids did not match expectation";
        }
        return true;
    }

private:
    PhaseGateCase m_scenario;
};

} // namespace


void add_tests_StorySavePolicy(UnitTestDatabase& database){
    for (const EffectiveModeCase& scenario : EFFECTIVE_MODE_CASES){
        database.add<Test_EffectiveSaveMode>(scenario);
    }
    for (const PhaseGateCase& scenario : PHASE_GATE_CASES){
        database.add<Test_PhaseGated>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
