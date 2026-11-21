/*  PokemonLZA AutoStory - Actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure logic tests for AutoStory actions: timed walks and menu text-speed setup.
 *
 */

#include <chrono>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStory_Tests.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryWalks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;

namespace{

template <typename Case, UnitTestResult (*check)(const Case&, CancellableScope&)>
class ActionCaseTest : public UnitTest{
public:
    ActionCaseTest(std::string_view prefix, const Case& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - {}: {}", prefix, test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check(m_case, scope);
    }

private:
    const Case& m_case;
};


ScreenObservation look(ScreenEvents events, std::vector<std::string_view> readings, Milliseconds at){
    ScreenObservation observation;
    observation.when = WallClock::min() + at;
    observation.events = events;
    for (std::string_view reading : readings){
        observation.readings[std::string(reading)] = 1;
    }
    return observation;
}

ScreenObservation overworld(Milliseconds at){
    return look({ScreenEvent::PLAYER_CAN_MOVE}, /*readings=*/{}, at);
}

ScreenObservation card(Milliseconds at){
    return look(/*events=*/{}, {TUTORIAL_CARD_READING}, at);
}

ScreenObservation platform(Milliseconds at){
    return look(/*events=*/{}, {CONTROLS_HINT_READING}, at);
}

ScreenObservation options(Milliseconds at){
    return look(/*events=*/{}, /*readings=*/{}, at);
}

std::string non_poll_inputs(const std::vector<std::string>& texts){
    std::string text;
    for (const std::string& part : texts){
        if (part == "wait 250ms"){
            continue;
        }
        if (!text.empty()){
            text += ", ";
        }
        text += part;
    }
    return text;
}


//  --- walk_until ---

struct WalkCase{
    std::string name;
    struct Input{
        StoryMove move;
        ScreenEvent end;
        std::vector<ScreenObservation> looks;
    };
    Input input;
    struct Expectation{
        std::string failure;
    };
    Expectation expectation;
};

const WalkCase WALK_CASES[] = {
    {
        .name = "white fade stops the last leg early",
        .input = {
            .move = {.legs = {{.turn = 0, .walk = 500ms}, {.turn = 90, .walk = 1000ms}}},
            .end = ScreenEvent::FADE_WHITE,
            .looks = {
                platform(0ms), platform(250ms),
                look({ScreenEvent::FADE_WHITE}, /*readings=*/{}, 500ms),
            },
        },
        .expectation = {.failure = ""},
    },
    {
        .name = "missing end event after budget fails with walk_no_end",
        .input = {
            .move = {.legs = {{.turn = 0, .walk = 250ms}}},
            .end = ScreenEvent::FADE_WHITE,
            .looks = {platform(0ms), platform(250ms), platform(500ms)},
        },
        .expectation = {.failure = "walk_no_end"},
    },
};

UnitTestResult check_walk(const WalkCase& test, CancellableScope& scope){
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    const std::optional<Pokemon::StoryFailure> outcome = walk_until(
        backend, "walk", test.input.move, {.end = test.input.end}
    );
    const std::string code = outcome ? outcome->code : std::string();
    if (code != test.expectation.failure){
        return std::format("expected failure \"{}\", got \"{}\"", test.expectation.failure, outcome ? std::format("{}", *outcome) : std::string("ok"));
    }
    return true;
}


//  --- set_text_speed_fast ---

struct TextSpeedCase{
    std::string name;
    struct Input{
        std::vector<ScreenObservation> looks;
    };
    Input input;
    struct Expectation{
        std::string failure;
        std::string inputs;
    };
    Expectation expectation;
};

constexpr std::string_view TEXT_SPEED_ROWS = "press L, press Up, press A, press Down, press Right";
constexpr std::string_view TEXT_SPEED_OPEN = "press X, wait 2000ms";

const TextSpeedCase TEXT_SPEED_CASES[] = {
    {
        //  console B, 2026-11-19, 01.01 3/3: a second A on a fading card opened the bag.
        .name = "a tutorial card fading out after its A gets no second A",
        .input = {.looks = {
            card(Milliseconds(0)), options(Milliseconds(1500)),
            options(Milliseconds(2500)), platform(Milliseconds(3500)), platform(Milliseconds(3750)),
        }},
        .expectation = {
            .failure = "",
            .inputs = std::format("{}, press A, wait 1500ms, {}, press B", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
    {
        .name = "a menu with no tutorial card gets the rows at once",
        .input = {.looks = {options(Milliseconds(0)), overworld(Milliseconds(1000)), overworld(Milliseconds(1250))}},
        .expectation = {
            .failure = "",
            .inputs = std::format("{}, {}", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
    {
        .name = "a screen no detector reads after the rows, like a stray bag, gets B until the overworld",
        .input = {.looks = {
            options(Milliseconds(0)), options(Milliseconds(1000)), options(Milliseconds(2000)),
            options(Milliseconds(3000)), overworld(Milliseconds(4000)), overworld(Milliseconds(4250)),
        }},
        .expectation = {
            .failure = "",
            .inputs = std::format("{}, {}, press B, press B, press B", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
    {
        .name = "never back on the overworld: the step times out",
        .input = {.looks = {options(Milliseconds(0)), options(Milliseconds(1000)), options(Milliseconds(31500))}},
        .expectation = {
            .failure = "text_speed_timeout",
            .inputs = std::format("{}, {}, press B", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
};

UnitTestResult check_text_speed(const TextSpeedCase& test, CancellableScope& scope){
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    const std::optional<Pokemon::StoryFailure> outcome = set_text_speed_fast(backend);
    if ((outcome ? outcome->code : std::string()) != test.expectation.failure){
        return std::format("expected failure \"{}\", got \"{}\"", test.expectation.failure, (outcome ? std::format("{}", *outcome) : std::string("ok")));
    }
    const std::string inputs = non_poll_inputs(backend.inputs());
    if (inputs != test.expectation.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", test.expectation.inputs, inputs);
    }
    return true;
}

} // namespace


void add_tests_AutoStoryActions(UnitTestDatabase& database){
    for (const WalkCase& test : WALK_CASES){
        database.add<ActionCaseTest<WalkCase, check_walk>>("walk_until", test);
    }
    for (const TextSpeedCase& test : TEXT_SPEED_CASES){
        database.add<ActionCaseTest<TextSpeedCase, check_text_speed>>("set_text_speed_fast", test);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
