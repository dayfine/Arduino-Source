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
    //  Whether the lead's panel shows (ZONE_PANEL_READING); none: not read.
    std::optional<bool> panel = std::nullopt;
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
        if (look.panel){
            observation.readings[std::string(ZONE_PANEL_READING)] = *look.panel ? 1 : 0;
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

std::vector<std::string> concat(std::vector<std::string> first, const std::vector<std::string>& second){
    first.insert(first.end(), second.begin(), second.end());
    return first;
}

//  Reads `pins` in turn, the last repeating; none when `pins` is empty.
PinReader pins_of(const std::vector<MapPoint>& pins){
    if (pins.empty()){
        return PinReader();
    }
    return [pins, next = size_t(0)]() mutable -> std::optional<MapPoint>{
        const MapPoint pin = pins[std::min(next, pins.size() - 1)];
        next++;
        return pin;
    };
}


//  The map pin on the street outside the far gate (03.08b's menu save,
//  console B 2026-11-17 01:06), and at Wild Zone 1's 60 catch spot.
constexpr MapPoint OUT_OF_ZONE_PIN{.x = 401, .y = 225};
constexpr MapPoint IN_ZONE_PIN{.x = 386, .y = 252};


struct GateWalkCase{
    std::string name;
    struct Input{
        //  In order; the last repeats.
        std::vector<Look> looks;
        Milliseconds timeout = 1000ms;
        //  The map pin's reads, in order, the last repeating; none: no reader.
        std::vector<MapPoint> pins = {};
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
        //  The user, 2026-11-16: "if we did walk out the zone, that's a
        //  finish regardless what battle detector says".
        .name = "out of the zone with no prompt: done",
        .input = {.looks = {{.gate_x = 600, .panel = true}, {.panel = false}, {.panel = false}}, .pins = {OUT_OF_ZONE_PIN}},
        .expectation = {.failure = std::nullopt, .inputs = {slice(6), slice(6)}},
    },
    {
        //  console B, 2026-11-18 13:37:15 (03.08b zone_1_to_far_gate): the
        //  lead's panel was gone on 2 looks 500 ms into the walk, still in
        //  the zone; the walk ended there, and 03.08b saved in the zone.
        .name = "the lead's panel gone but the map pin in the zone: walks on to the prompt",
        .input = {
            .looks = {{.gate_x = 600, .panel = true}, {.panel = false}, {.panel = false}, {.prompt = true}},
            .pins = {IN_ZONE_PIN},
        },
        .expectation = {.failure = std::nullopt, .inputs = {slice(6), slice(6), slice(6)}},
    },
    {
        .name = "the lead's panel gone with no map pin reader: walks on to the prompt",
        .input = {.looks = {{.gate_x = 600, .panel = true}, {.panel = false}, {.panel = false}, {.prompt = true}}},
        .expectation = {.failure = std::nullopt, .inputs = {slice(6), slice(6), slice(6)}},
    },
    {
        .name = "a battle read out of the zone does not hold the walk",
        .input = {
            .looks = {{.gate_x = 500, .panel = true}, {.battle = true, .panel = false}}, .timeout = 15000ms,
            .pins = {OUT_OF_ZONE_PIN},
        },
        .expectation = {.failure = std::nullopt, .inputs = {slice(0), slice(0)}},
    },
    {
        //  A walk toward a zone from outside it never had the panel.
        .name = "no panel from the start is not out of the zone",
        .input = {.looks = {{.gate_x = 500, .panel = false}, {.gate_x = 500, .panel = false}, {.prompt = true}}},
        .expectation = {.failure = std::nullopt, .inputs = {slice(0), slice(0)}},
    },
    {
        .name = "one look without the panel is not out of the zone",
        .input = {.looks = {{.gate_x = 500, .panel = true}, {.gate_x = 500, .panel = false}, {.gate_x = 500, .panel = true}, {.prompt = true}}},
        .expectation = {.failure = std::nullopt, .inputs = {slice(0), slice(0), slice(0)}},
    },
    {
        //  console B, 2026-11-16 19:24 (03.08b run 3): a battle read
        //  walked on 20 s into the zone's pillar. The pin read after 2 s
        //  of walking on has not moved: re-aimed, the gate steers again.
        .name = "walking on with the pin still: steers by the gate again",
        .input = {
            .looks = {
                {.gate_x = 700, .battle = true}, {.gate_x = 700, .battle = true}, {.gate_x = 700, .battle = true},
                {.gate_x = 700, .battle = true}, {.gate_x = 700, .battle = true}, {.gate_x = 700, .battle = true},
                {.gate_x = 700, .battle = true}, {.gate_x = 700, .battle = true}, {.gate_x = 700, .battle = true},
                {.prompt = true},
            },
            .timeout = 15000ms,
            .pins = {{.x = 367, .y = 257}},
        },
        .expectation = {.failure = std::nullopt, .inputs = concat(slices(8, 0), {slice(12)})},
    },
    {
        .name = "walking on with the pin moving: walks on",
        .input = {
            .looks = battle_then(16, {{.gate_x = 500}, {.prompt = true}}), .timeout = 15000ms,
            .pins = {{.x = 384, .y = 252}, {.x = 374, .y = 252}, {.x = 364, .y = 252}},
        },
        .expectation = {.failure = std::nullopt, .inputs = slices(17, 0)},
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
        backend, MinimapArrowReader(), LookReader(), pins_of(test.input.pins),
        GateWalk{.heading = std::nullopt, .timeout = test.input.timeout}
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


struct OutOfZoneCase{
    std::string name;
    struct Input{
        //  The map pin's reads, in order, the last repeating; none: no reader.
        std::vector<MapPoint> pins;
    };
    Input input;
    struct Expectation{
        //  The failure; none: out of the zone.
        std::optional<std::string> failure;
    };
    Expectation expectation;
};

const OutOfZoneCase OUT_OF_ZONE_CASES[] = {
    {
        .name = "out of the zone on every read",
        .input = {.pins = {OUT_OF_ZONE_PIN}},
        .expectation = {.failure = std::nullopt},
    },
    {
        //  console B, 2026-11-18 13:37: 03.08b saved in Wild Zone 1.
        .name = "in the zone: no save there",
        .input = {.pins = {IN_ZONE_PIN}},
        .expectation = {.failure = "in_wild_zone"},
    },
    {
        .name = "out on the first read, in on the next: in the zone",
        .input = {.pins = {OUT_OF_ZONE_PIN, IN_ZONE_PIN}},
        .expectation = {.failure = "in_wild_zone"},
    },
    {
        .name = "no map pin reader: not known to be out",
        .input = {.pins = {}},
        .expectation = {.failure = "zone_unread"},
    },
};

UnitTestResult check_out_of_zone(const OutOfZoneCase& test, CancellableScope& scope){
    FakeBackend backend(Pokemon::FakeStoryScript<ZonePose>{.looks = {}, .kind = Pokemon::StoryBackendKind::SCRIPT}, scope);
    const Pokemon::Outcome outcome = check_out_of_zone_1(backend, pins_of(test.input.pins));
    const std::optional<std::string> failure = outcome.succeeded() ? std::nullopt : std::optional<std::string>(outcome.failure);
    if (failure != test.expectation.failure){
        return std::format("Expected {}, got {}.", test.expectation.failure.value_or("a pass"), to_string(outcome));
    }
    return true;
}

class Test_OutOfZone : public UnitTest{
public:
    explicit Test_OutOfZone(const OutOfZoneCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - check_out_of_zone_1: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_out_of_zone(m_test, scope);
    }

private:
    const OutOfZoneCase& m_test;
};

} // namespace


void add_tests_AutoStoryGateWalk(UnitTestDatabase& database){
    for (const GateWalkCase& test : GATE_WALK_CASES){
        database.add<Test_GateWalk>(test);
    }
    for (const OutOfZoneCase& test : OUT_OF_ZONE_CASES){
        database.add<Test_OutOfZone>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
