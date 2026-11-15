/*  PokemonLZA AutoStory - Walk to a wild zone's gate Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  walk_to_zone_gate_entrance() on the engine's scripted backend
 *  (FakeStoryBackend): each look carries the gate readings a console's
 *  zone_gate_reader() would set, and the A prompt.
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryGame.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStoryGateWalk.h"
#include "PokemonLZA_AutoStoryGateWalk_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::Observation;
using Pokemon::ZonePose;


namespace{

using FakeBackend = Pokemon::FakeStoryBackend<ZonePose>;

//  What a look shows.
struct Look{
    //  The gate's middle, in thousandths of the width; none: no gate.
    std::optional<int> gate_x = std::nullopt;
    bool prompt = false;
    //  The minimap's heading; none: not read.
    std::optional<int> heading = std::nullopt;
};

std::vector<Observation<ZonePose>> looks_of(const std::vector<Look>& looks){
    std::vector<Observation<ZonePose>> out;
    const WallClock start = current_time();
    for (const Look& look : looks){
        Observation<ZonePose> observation;
        observation.when = start + Milliseconds(250) * out.size();
        if (look.gate_x){
            observation.readings[std::string(ZONE_GATE_X_READING)] = *look.gate_x;
            observation.readings[std::string(ZONE_GATE_WIDTH_READING)] = 150;
        }
        if (look.heading){
            observation.readings[std::string(Pokemon::STORY_HEADING)] = *look.heading;
        }
        if (look.prompt){
            observation.events.set(ScreenEvent::INTERACT_PROMPT, true);
        }
        out.push_back(std::move(observation));
    }
    return out;
}

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += (text.empty() ? "" : ", ") + line;
    }
    return text;
}

std::string slice(int degrees){
    return std::format("stick {}deg 250ms walk", degrees);
}


struct GateWalkCase{
    std::string name;
    struct Input{
        //  In order; the last repeats.
        std::vector<Look> looks;
        Milliseconds timeout = 1000ms;
    };
    Input input;
    struct Expectation{
        //  The failure; none: the walk succeeds.
        std::optional<std::string> failure;
        std::vector<std::string> inputs;
        //  In the failure's message, when there is one.
        std::string message = "";
    };
    Expectation expectation;
};

const GateWalkCase GATE_WALK_CASES[] = {
    {
        .name = "at the prompt already: no walk",
        .input = {.looks = {{.gate_x = 500, .prompt = true}}},
        .expectation = {.failure = std::nullopt, .inputs = {}},
    },
    {
        .name = "steers toward the gate until the prompt",
        .input = {.looks = {{.gate_x = 700}, {.gate_x = 600}, {.gate_x = 505}, {.prompt = true}}},
        .expectation = {.failure = std::nullopt, .inputs = {slice(12), slice(6), slice(0)}},
    },
    {
        .name = "a gate lost up close keeps the last angle",
        .input = {.looks = {{.gate_x = 250}, {}, {}, {.prompt = true}}},
        .expectation = {.failure = std::nullopt, .inputs = {slice(-15), slice(-15), slice(-15)}},
    },
    {
        .name = "no gate in the first 3 seconds fails",
        .input = {.looks = {{}}, .timeout = 15000ms},
        .expectation = {
            .failure = "gate_not_seen", .inputs = std::vector<std::string>(12, slice(0)),
            .message = "no gate on 13 looks",
        },
    },
    {
        .name = "a gate late in the first second is seen",
        .input = {.looks = {{}, {}, {}, {.gate_x = 400}, {.prompt = true}}},
        .expectation = {.failure = std::nullopt, .inputs = {slice(0), slice(0), slice(0), slice(-6)}},
    },
    {
        .name = "no prompt by the timeout fails with the last gate read",
        .input = {.looks = {{.gate_x = 510}}, .timeout = 1000ms},
        .expectation = {
            .failure = "no_prompt", .inputs = {slice(0), slice(0), slice(0), slice(0)},
            .message = "gate on 5 of 5 looks, last at x = 0.510 width 0.150",
        },
    },
};

UnitTestResult check_gate_walk(const GateWalkCase& test, CancellableScope& scope){
    FakeBackend backend(Pokemon::FakeStoryScript<ZonePose>{
        .looks = looks_of(test.input.looks), .battle = Pokemon::outcome_ok(), .kind = Pokemon::StoryBackendKind::SCRIPT,
    }, scope);
    const Pokemon::Outcome outcome = walk_to_zone_gate_entrance(
        backend, PlzaReader(), PlzaReader(), GateWalk{.heading = std::nullopt, .timeout = test.input.timeout}
    );
    const std::optional<std::string> failure = outcome.succeeded() ? std::nullopt : std::optional<std::string>(outcome.failure);
    if (failure != test.expectation.failure){
        return std::format("Expected {}, got {}.", test.expectation.failure.value_or("a pass"), to_string(outcome));
    }
    if (failure && to_string(outcome).find(test.expectation.message) == std::string::npos){
        return std::format("Expected \"{}\" in {}.", test.expectation.message, to_string(outcome));
    }
    if (backend.inputs() != test.expectation.inputs){
        return std::format("Expected inputs [{}], got [{}].", joined(test.expectation.inputs), joined(backend.inputs()));
    }
    return true;
}


//  The step `name` of checkpoint `id`, alone in a checkpoint of its own;
//  none when the story has no such step.
std::optional<Pokemon::StoryCheckpoint> step_of(std::string_view id, std::string_view name){
    for (const Pokemon::StoryCheckpoint& row : story_checkpoints()){
        if (row.id != id){
            continue;
        }
        for (const Pokemon::StoryStep& step : row.steps){
            if (step.name == name){
                Pokemon::StoryCheckpoint one;
                one.id = row.id;
                one.title = row.title;
                one.steps.push_back(step);
                return one;
            }
        }
    }
    return std::nullopt;
}

//  A checkpoint's to_zone_1_gate through PLZA's engine, on looks that read
//  30 degrees left of the plan's heading, then the heading, then the gate
//  dead ahead, then the prompt.
struct PlanStepCase{
    std::string name;
    struct Input{
        std::string_view checkpoint;
    };
    Input input;
    struct Expectation{
        std::vector<std::string> inputs;
    };
    Expectation expectation;
};

//  Both face the plan's heading by the minimap first (a turn of 30), then
//  walk the gate's way until the prompt.
const PlanStepCase PLAN_STEP_CASES[] = {
    {
        .name = "03.06 to_zone_1_gate faces the street, then the gate",
        .input = {.checkpoint = "03.06"},
        .expectation = {.inputs = {"press L", "stick 30deg 150ms walk, release 100ms", slice(0)}},
    },
    {
        .name = "03.07d to_zone_1_gate faces the street, then the gate",
        .input = {.checkpoint = "03.07d"},
        .expectation = {.inputs = {"press L", "stick 30deg 150ms walk, release 100ms", slice(0)}},
    },
};

UnitTestResult check_plan_step(const PlanStepCase& test, CancellableScope& scope){
    const std::string_view id = test.input.checkpoint;
    const std::optional<Pokemon::StoryCheckpoint> row = step_of(id, "to_zone_1_gate");
    if (!row){
        return std::format("{} has no to_zone_1_gate step.", id);
    }
    const Pokemon::StoryStep& step = row->steps[0];
    const auto heading = step.args.find("heading");
    if (step.action != "walk_to_zone_gate_entrance" || heading == step.args.end()){
        return std::format("{}: expected walk_to_zone_gate_entrance with a heading, got {}.", id, step.action);
    }
    const int facing = (int)std::get<int64_t>(heading->second);
    FakeBackend backend(Pokemon::FakeStoryScript<ZonePose>{
        .looks = looks_of({{.heading = (facing + 330) % 360}, {.heading = facing}, {.gate_x = 500}, {.prompt = true}}),
        .battle = Pokemon::outcome_ok(), .kind = Pokemon::StoryBackendKind::SCRIPT,
    }, scope);
    const Pokemon::StoryGame<ZonePose> game = make_plza_story_game(StarterChoice::CHIKORITA);
    Pokemon::StoryEngine<ZonePose> engine(game, backend);
    Pokemon::RunState state;
    const Pokemon::StoryEngineReport report = engine.run_checkpoint(*row, Pokemon::SavePoint::RUN_END, state);
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return std::format("{}: expected a pass, got {}.", id, to_string(report.run.failure));
    }
    if (backend.inputs() != test.expectation.inputs){
        return std::format("{}: expected inputs [{}], got [{}].", id, joined(test.expectation.inputs), joined(backend.inputs()));
    }
    return true;
}


class Test_GateWalk : public UnitTest{
public:
    explicit Test_GateWalk(const GateWalkCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - walk_to_zone_gate_entrance: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_gate_walk(m_test, scope);
    }

private:
    const GateWalkCase& m_test;
};

class Test_GatePlanStep : public UnitTest{
public:
    explicit Test_GatePlanStep(const PlanStepCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - walk_to_zone_gate_entrance: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_plan_step(m_test, scope);
    }

private:
    const PlanStepCase& m_test;
};

} // namespace


void add_tests_AutoStoryGateWalk(UnitTestDatabase& database){
    for (const GateWalkCase& test : GATE_WALK_CASES){
        database.add<Test_GateWalk>(test);
    }
    for (const PlanStepCase& test : PLAN_STEP_CASES){
        database.add<Test_GatePlanStep>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
