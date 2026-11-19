/*  PokemonLZA AutoStory - Menu save Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryMenuReadings.h"
#include "PokemonLZA_AutoStoryMenuSave.h"
#include "PokemonLZA_AutoStoryMenuSave_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::Observation;
using Pokemon::ZonePose;


Observation<ZonePose> look(ScreenEvents events, std::vector<std::string_view> readings, Milliseconds at){
    Observation<ZonePose> observation;
    observation.when = WallClock::min() + at;
    observation.events = events;
    for (std::string_view reading : readings){
        observation.readings[std::string(reading)] = 1;
    }
    return observation;
}

Observation<ZonePose> overworld(Milliseconds at){
    return look({ScreenEvent::PLAYER_CAN_MOVE}, /*readings=*/{}, at);
}

Observation<ZonePose> card(Milliseconds at){
    return look(/*events=*/{}, {TUTORIAL_CARD_READING}, at);
}

Observation<ZonePose> text(Milliseconds at){
    return look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, at);
}

Observation<ZonePose> main_menu(Milliseconds at){
    return look(/*events=*/{}, {MAIN_MENU_READING}, at);
}

Observation<ZonePose> save_button(Milliseconds at){
    return look(/*events=*/{}, {MENU_SAVE_BUTTON_READING}, at);
}

Observation<ZonePose> choice(Milliseconds at){
    return look({ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX}, /*readings=*/{}, at);
}

Observation<ZonePose> home(Milliseconds at){
    return look({ScreenEvent::SWITCH_HOME_MENU}, /*readings=*/{}, at);
}

std::vector<Observation<ZonePose>> twice(std::vector<Observation<ZonePose> (*)(Milliseconds)> screens){
    std::vector<Observation<ZonePose>> looks;
    Milliseconds at(0);
    for (const auto& screen : screens){
        for (int repeat = 0; repeat < 2; repeat++){
            looks.push_back(screen(at));
            at += Milliseconds(250);
        }
    }
    return looks;
}

std::string joined(const std::vector<std::string>& texts){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : ", ") + part;
    }
    return text;
}


struct MenuSaveCase{
    std::string name;
    struct Input{
        std::vector<Observation<ZonePose>> looks;
    };
    Input input;
    struct Expectation{
        //  Empty: saved.
        std::string failure;
        std::string inputs;
    };
    Expectation expectation;
};

const MenuSaveCase MENU_SAVE_CASES[] = {
    {
        //  console B, 2026-11-16 20:05: 03.08b's save, the save file's
        //  first X, hung for 2 minutes on the menu's tutorial card.
        .name = "the first X's tutorial card gets A, then R saves",
        .input = {.looks = twice({overworld, card, main_menu, save_button, text, main_menu, overworld})},
        .expectation = {
            .failure = "",
            .inputs = "wait 250ms, press X, wait 250ms, press A, wait 250ms, press R, wait 250ms, press A, "
                "wait 250ms, press B, wait 250ms, press B",
        },
    },
    {
        .name = "a text box after R before the confirmation gets A",
        .input = {.looks = twice({overworld, main_menu, text, save_button, text, overworld})},
        .expectation = {
            .failure = "",
            .inputs = "wait 250ms, press X, wait 250ms, press R, wait 250ms, press A, wait 250ms, press A, wait 250ms, press B",
        },
    },
    {
        .name = "the main menu back after a confirmed choice, with no saved text, is saved",
        .input = {.looks = twice({overworld, main_menu, choice, main_menu, overworld})},
        .expectation = {
            .failure = "",
            .inputs = "wait 250ms, press X, wait 250ms, press R, wait 250ms, press A, wait 250ms, press B",
        },
    },
    {
        .name = "the main menu that stays after R: R again 2 s later",
        .input = {.looks = {
            main_menu(Milliseconds(0)), main_menu(Milliseconds(250)), main_menu(Milliseconds(500)),
            main_menu(Milliseconds(750)), main_menu(Milliseconds(2250)), main_menu(Milliseconds(31000)),
        }},
        .expectation = {.failure = "menu_save_timeout", .inputs = "wait 250ms, press R, wait 250ms, wait 250ms, press R"},
    },
    {
        .name = "a screen on one look only is not pressed",
        .input = {.looks = {
            overworld(Milliseconds(0)), card(Milliseconds(250)),
            overworld(Milliseconds(500)), overworld(Milliseconds(31000)),
        }},
        .expectation = {.failure = "menu_save_timeout", .inputs = "wait 250ms, wait 250ms, wait 250ms"},
    },
    {
        .name = "a menu that never opens: X again 2 s later, then the step times out",
        .input = {.looks = {
            overworld(Milliseconds(0)), overworld(Milliseconds(250)), overworld(Milliseconds(500)),
            overworld(Milliseconds(750)), overworld(Milliseconds(2250)), overworld(Milliseconds(31000)),
        }},
        .expectation = {.failure = "menu_save_timeout", .inputs = "wait 250ms, press X, wait 250ms, wait 250ms, press X"},
    },
    {
        .name = "the Switch's HOME menu: the game is not running",
        .input = {.looks = {home(Milliseconds(0))}},
        .expectation = {.failure = std::string(Pokemon::GAME_NOT_RUNNING), .inputs = ""},
    },
};

UnitTestResult check_menu_save(const MenuSaveCase& test, CancellableScope& scope){
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks}, scope);
    const Pokemon::Outcome outcome = save_from_menu(backend);
    if (outcome.failure != test.expectation.failure){
        return std::format("expected failure \"{}\", got \"{}\"", test.expectation.failure, Pokemon::to_string(outcome));
    }
    const std::string inputs = joined(backend.inputs());
    if (inputs != test.expectation.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", test.expectation.inputs, inputs);
    }
    return true;
}


class MenuSaveTest : public UnitTest{
public:
    explicit MenuSaveTest(MenuSaveCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - save_from_menu: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_menu_save(m_case, scope);
    }
private:
    MenuSaveCase m_case;
};

} // namespace


void add_tests_AutoStoryMenuSave(UnitTestDatabase& database){
    for (const MenuSaveCase& test : MENU_SAVE_CASES){
        database.add<MenuSaveTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
