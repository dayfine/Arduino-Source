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
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
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
    //  A wild battle's HUD (BATTLE_STARTED).
    bool battle = false;
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
        if (look.prompt){
            observation.events.set(ScreenEvent::INTERACT_PROMPT, true);
        }
        if (look.battle){
            observation.events.set(ScreenEvent::BATTLE_STARTED, true);
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

//  `count` looks of a wild battle, then `after`.
std::vector<Look> battle_then(size_t count, std::vector<Look> after){
    std::vector<Look> looks(count, Look{.battle = true});
    looks.insert(looks.end(), after.begin(), after.end());
    return looks;
}

std::vector<std::string> slices(size_t count, int degrees){
    return std::vector<std::string>(count, slice(degrees));
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
        //  console B, 2026-11-16 18:44:57 (03.08b zone_1_to_far_gate): a
        //  wild Mareep engaged at the zone's pillar, the gate never read
        //  in 3 s, and the walk failed as gate_not_seen. A battle's looks
        //  walk on at the last angle and do not count toward it.
        .name = "a wild battle at the start: walks on, then the gate",
        .input = {.looks = battle_then(16, {{.gate_x = 500}, {.prompt = true}}), .timeout = 15000ms},
        .expectation = {.failure = std::nullopt, .inputs = slices(17, 0)},
    },
    {
        .name = "a wild battle that does not end in 20 s fails",
        .input = {.looks = battle_then(1, {}), .timeout = 15000ms},
        .expectation = {.failure = "wild_battle", .inputs = slices(80, 0), .message = "20000 ms"},
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
        .looks = looks_of(test.input.looks), .kind = Pokemon::StoryBackendKind::SCRIPT, .battle = Pokemon::outcome_ok(),
    }, scope);
    const Pokemon::Outcome outcome = walk_to_zone_gate_entrance(
        backend, MinimapArrowReader(), LookReader(), GateWalk{.heading = std::nullopt, .timeout = test.input.timeout}
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

} // namespace


void add_tests_AutoStoryGateWalk(UnitTestDatabase& database){
    for (const GateWalkCase& test : GATE_WALK_CASES){
        database.add<Test_GateWalk>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
