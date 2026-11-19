/*  PokemonLZA AutoStory - Wild Zone 1 map Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  check_out_of_zone_1() on the engine's scripted backend
 *  (FakeStoryBackend), its map pin reads scripted.
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryZone1Map.h"
#include "PokemonLZA_AutoStoryZone1Map_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{



namespace{

using FakeBackend = Pokemon::FakeStoryBackend;

//  Reads `pins` in turn, the last repeating; none when `pins` is empty.
MoveOnlyFunction<std::optional<MapPoint>()> pins_of(const std::vector<MapPoint>& pins){
    if (pins.empty()){
        return nullptr;
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


struct OutOfZoneCase{
    std::string name;
    struct Input{
        //  The last repeating; none: no reader.
        std::vector<MapPoint> pins;
    };
    Input input;
    struct Expectation{
        //  None: out of the zone.
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
    FakeBackend backend(Pokemon::FakeStoryScript{.looks = {}}, scope);
    const std::optional<Pokemon::StoryFailure> outcome = check_out_of_zone_1(backend, pins_of(test.input.pins));
    const std::optional<std::string> failure = !outcome ? std::nullopt : std::optional<std::string>(outcome->code);
    if (failure != test.expectation.failure){
        return std::format("Expected {}, got {}.", test.expectation.failure.value_or("a pass"), (outcome ? std::format("{}", *outcome) : std::string("ok")));
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


void add_tests_AutoStoryZone1Map(UnitTestDatabase& database){
    for (const OutOfZoneCase& test : OUT_OF_ZONE_CASES){
        database.add<Test_OutOfZone>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
