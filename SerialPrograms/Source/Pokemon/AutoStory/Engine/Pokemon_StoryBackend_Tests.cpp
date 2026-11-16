/*  Story Backend Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <string>
#include <utility>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_ScreenObservation.h"
#include "Pokemon_StoryBackend.h"
#include "Pokemon_StoryFakeBackend.h"
#include "Pokemon_ZonePose.h"
#include "Pokemon_StoryBackend_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

using namespace std::chrono_literals;


//  A look at one piece: what it gives as text.
struct BackendCase{
    std::string name;
    struct Input{
        std::function<std::string(CancellableScope& scope)> read;
    };
    Input input;
    struct Expectation{
        std::string text;
    };
    Expectation expectation;
};

std::string observation_text(ScreenEvents events){
    return describe(screen_observation(events).events);
}

std::string matches_text(){
    const ZonePose city{.zone = "LumioseCity"};
    const ZonePose hotel{.zone = "HotelZ"};
    const ZonePose any{};
    return std::format(
        "{} {} {} {}",
        pose_matches(city, city), pose_matches(city, hotel), pose_matches(hotel, any), pose_matches(any, city)
    );
}

//  The scripted backend's inputs and looks.
std::string fake_text(CancellableScope& scope){
    Observation<ZonePose> fade;
    fade.events = {ScreenEvent::FADE_WHITE};
    FakeStoryBackend<ZonePose> backend({.looks = {Observation<ZonePose>{}, fade}}, scope);
    backend.press(NintendoSwitch::BUTTON_A, /*hold=*/80ms, /*release=*/80ms);
    backend.tap(Direction::EAST);
    backend.hold(Direction::NORTH, /*duration=*/400ms, StoryPace::RUN);
    backend.stick(/*degrees=*/90, /*duration=*/150ms, /*release=*/300ms, StoryPace::WALK);
    backend.wait(100ms);
    std::string looks;
    for (size_t c = 0; c < 3; c++){
        looks += std::format("{}{}", looks.empty() ? "" : " / ", describe(backend.observe().events));
    }
    std::string text;
    for (const std::string& input : backend.inputs()){
        text += std::format("{}, ", input);
    }
    return text + looks;
}

//  The scripted backend's battles: recorded, ending as scripted (ok when
//  the script names no outcome).
std::string fake_battle_text(CancellableScope& scope){
    FakeStoryBackend<ZonePose> won({}, scope);
    FakeStoryBackend<ZonePose> lost({.battle = outcome_failed("battle_lost", "the party fainted")}, scope);
    const Outcome first = won.battle();
    const Outcome second = lost.battle();
    return std::format(
        "{} {} / {} {}", won.inputs().front(), first.succeeded() ? "ok" : first.failure,
        lost.inputs().front(), second.succeeded() ? "ok" : second.failure
    );
}

const BackendCase CASES[] = {
    {
        .name = "a look keeps the engine's events",
        .input = {.read = [](CancellableScope&){
            return observation_text({ScreenEvent::FADE_WHITE, ScreenEvent::LOCATION_BANNER, ScreenEvent::SWITCH_HOME_MENU});
        }},
        .expectation = {.text = "white fade"},
    },
    {
        .name = "a choice box in a look is also a dialog box",
        .input = {.read = [](CancellableScope&){ return observation_text({ScreenEvent::CHOICE_BOX}); }},
        .expectation = {.text = "dialog box, choice box"},
    },
    {
        .name = "a zone pose matches its zone, or any zone when the target names none",
        .input = {.read = [](CancellableScope&){ return matches_text(); }},
        .expectation = {.text = "true false true false"},
    },
    {
        .name = "the scripted backend records inputs and repeats its last look",
        .input = {.read = fake_text},
        .expectation = {.text = "press A, tap east, hold north 400ms run, stick 90deg 150ms walk, release 300ms, wait 100ms, "
            "nothing / white fade / white fade"},
    },
    {
        .name = "the scripted backend records a battle and ends it as scripted",
        .input = {.read = fake_battle_text},
        .expectation = {.text = "battle ok / battle battle_lost"},
    },
};


class StoryBackendTest : public UnitTest{
public:
    explicit StoryBackendTest(BackendCase test_case)
        : UnitTest(std::format("Pokemon::StoryBackend - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string got = m_case.input.read(scope);
        if (got != m_case.expectation.text){
            return std::format("expected \"{}\", got \"{}\"", m_case.expectation.text, got);
        }
        return true;
    }

private:
    BackendCase m_case;
};

} // namespace


void add_tests_StoryBackend(UnitTestDatabase& database){
    for (const BackendCase& test : CASES){
        database.add<StoryBackendTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
