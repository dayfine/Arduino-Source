/*  PokemonLZA AutoStory - Scene machine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The scene machine's states on sequences of events, as PLZA's backend
 *  observes them (plza_observation()): no frames needed.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryObservation.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::ScreenEvent;
using Pokemon::ScreenEvents;

namespace{

//  One read of the screen and what the machine should make of it.
struct Read{
    Milliseconds at;
    ScreenEvents events;
    bool still;
    SceneScreen expected;
    //  The controls hint (CONTROLS_HINT_READING) shows.
    bool hint = false;
    //  The advance chevron (ADVANCE_ARROW_READING) shows.
    bool arrow = false;
    //  The mission banner (MISSION_BANNER_READING) shows.
    bool banner = false;
    //  The mission complete card (MISSION_COMPLETE_READING) shows.
    bool complete = false;
};

//  Feeds `reads` to a timeline as the machine does (classify, then record
//  the read it acted on) and checks each state, until a terminal one.
UnitTestResult check_reads(const SceneMachineConfig& config, const std::vector<Read>& reads){
    const WallClock start = current_time();
    SceneTimeline timeline(config, start);
    for (const Read& read : reads){
        ScreenObservation look = plza_observation(read.events);
        look.when = start + read.at;
        if (read.still){
            look.events.add(PokemonAutomation::ScreenEvent::STILL);
        }
        if (read.hint){
            look.readings[std::string(CONTROLS_HINT_READING)] = 1;
        }
        if (read.arrow){
            look.readings[std::string(ADVANCE_ARROW_READING)] = 1;
        }
        if (read.banner){
            look.readings[std::string(MISSION_BANNER_READING)] = 1;
        }
        if (read.complete){
            look.readings[std::string(MISSION_COMPLETE_READING)] = 1;
        }
        const SceneScreen got = timeline.classify(look);
        if (got != read.expected){
            return std::format("at {} ms ({}): expected {}, got {}.",
                read.at.count(), Pokemon::describe(read.events),
                scene_screen_name(read.expected), scene_screen_name(got));
        }
        timeline.record(look);
    }
    return true;
}

SceneMachineConfig opening_config(Milliseconds min_length){
    SceneMachineConfig config;
    config.quiet = 8000ms;
    config.min_length = min_length;
    return config;
}

const ScreenEvents NOTHING{};
const ScreenEvents DIALOG{ScreenEvent::DIALOG_BOX};
const ScreenEvents CHOICE{ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX};
const ScreenEvents PARTY_BAR{ScreenEvent::PLAYER_CAN_MOVE};
const ScreenEvents WHITE{ScreenEvent::FADE_WHITE};
const ScreenEvents BLACK{ScreenEvent::FADE_BLACK};
const ScreenEvents BATTLE{ScreenEvent::BATTLE_STARTED};
const ScreenEvents PROMPT{ScreenEvent::INTERACT_PROMPT};

//  mash_to_move_hints' config: a minute quiet, blind A after 5 s.
SceneMachineConfig arrival_config(){
    SceneMachineConfig config;
    config.quiet = 60000ms;
    config.blind_press_after = 5000ms;
    return config;
}

//  The opening: a still frame past the quiet time does not end it before
//  the scene's minimum length; then it does.
UnitTestResult test_quiet_waits_for_min_length(Logger&){
    return check_reads(opening_config(60000ms), {
        {0ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {5000ms, DIALOG, false, SceneScreen::DIALOG},
        {10000ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {30000ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {59900ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {60000ms, NOTHING, true, SceneScreen::QUIET_END},
    });
}

//  A box restarts the quiet time; a fade counts as busy too.
UnitTestResult test_quiet_restarts_on_busy(Logger&){
    return check_reads(opening_config(0ms), {
        {0ms, DIALOG, false, SceneScreen::DIALOG},
        {7900ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {7950ms, WHITE, false, SceneScreen::FADE_WHITE},
        {15000ms, WHITE, true, SceneScreen::FADE_WHITE},
        {20000ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {22900ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {23000ms, NOTHING, true, SceneScreen::QUIET_END},
    });
}

//  The party bar ends the scene once it has held a second; a box between
//  lines starts the second again.
UnitTestResult test_party_bar_held(Logger&){
    return check_reads(opening_config(60000ms), {
        {0ms, PARTY_BAR, false, SceneScreen::PARTY_BAR},
        {500ms, PARTY_BAR, true, SceneScreen::PARTY_BAR},
        {600ms, DIALOG, false, SceneScreen::DIALOG},
        {800ms, PARTY_BAR, false, SceneScreen::PARTY_BAR},
        {1700ms, PARTY_BAR, true, SceneScreen::PARTY_BAR},
        {1800ms, PARTY_BAR, true, SceneScreen::CAN_MOVE},
    });
}

//  The controls hint ends the scene once it has held a second, ahead of
//  the quiet end; a box over it starts the second again.
UnitTestResult test_controls_hint_held(Logger&){
    return check_reads(opening_config(0ms), {
        {0ms, NOTHING, false, SceneScreen::HINT_SHOWING, true},
        {600ms, DIALOG, false, SceneScreen::DIALOG, true},
        {900ms, NOTHING, false, SceneScreen::HINT_SHOWING, true},
        {1800ms, NOTHING, true, SceneScreen::HINT_SHOWING, true},
        {9000ms, NOTHING, true, SceneScreen::CONTROLS_HINT, true},
    });
}

//  01.01's ad: subtitles with no box. The advance chevron is text to press
//  on; with nothing seen for 5 s, A every 2 s. An interact prompt is free
//  control like the controls hint: one look stops the presses, held a
//  second it ends the scene (01.02's battle area shows only the prompt).
UnitTestResult test_ad_subtitles(Logger&){
    return check_reads(arrival_config(), {
        {0ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {1000ms, NOTHING, false, SceneScreen::DIALOG, false, true},
        {1500ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {3000ms, NOTHING, true, SceneScreen::DIALOG, false, true},
        {7900ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {8000ms, NOTHING, true, SceneScreen::BLIND_PRESS},
        {9000ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {10000ms, NOTHING, false, SceneScreen::BLIND_PRESS},
        {12000ms, PROMPT, false, SceneScreen::HINT_SHOWING},
        {12100ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {14200ms, NOTHING, false, SceneScreen::HINT_SHOWING, true},
        {15000ms, NOTHING, true, SceneScreen::HINT_SHOWING, true},
        {15300ms, NOTHING, true, SceneScreen::CONTROLS_HINT, true},
    });
}

//  01.01's arrival (run 3, 2026-11-10 13:49): the last box, then "MAIN
//  MISSION 01" over the scene. The banner's first look waits, its second
//  ends the scene; a box between the two starts the count again. No blind
//  A while it shows.
UnitTestResult test_mission_banner_ends(Logger&){
    return check_reads(arrival_config(), {
        {0ms, DIALOG, false, SceneScreen::DIALOG},
        {500ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {6000ms, NOTHING, false, SceneScreen::BANNER_SHOWING, false, false, true},
        {6250ms, DIALOG, false, SceneScreen::DIALOG, false, false, true},
        {6500ms, NOTHING, false, SceneScreen::BANNER_SHOWING, false, false, true},
        {12000ms, NOTHING, true, SceneScreen::MISSION_BANNER, false, false, true},
    });
}

//  01.03's end (console B, 2026-11-10): the "MAIN MISSION 01 COMPLETE"
//  card waits for A. Each look at it presses A, and it is busy: no blind
//  press and no quiet end however long it shows, and a look with the
//  banner's reading too is the card. Then "MAIN MISSION 02" ends the scene
//  on its second look.
UnitTestResult test_mission_complete_presses_a(Logger&){
    SceneMachineConfig config = arrival_config();
    config.quiet = 8000ms;
    return check_reads(config, {
        {0ms, DIALOG, false, SceneScreen::DIALOG},
        {4000ms, NOTHING, false, SceneScreen::MISSION_COMPLETE, false, false, false, true},
        {9000ms, NOTHING, true, SceneScreen::MISSION_COMPLETE, false, false, false, true},
        {20000ms, NOTHING, true, SceneScreen::MISSION_COMPLETE, false, false, true, true},
        {20500ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {21000ms, NOTHING, false, SceneScreen::BANNER_SHOWING, false, false, true},
        {21250ms, NOTHING, true, SceneScreen::MISSION_BANNER, false, false, true},
    });
}

//  The banner ends only a scene after the overworld: B to a choice
//  presses on past it.
UnitTestResult test_mission_banner_only_overworld(Logger&){
    SceneMachineConfig config;
    config.goal = SceneMashGoal::CHOICE;
    config.text_button = BUTTON_B;
    return check_reads(config, {
        {0ms, NOTHING, false, SceneScreen::SCENE_MOVING, false, false, true},
        {500ms, NOTHING, false, SceneScreen::SCENE_MOVING, false, false, true},
        {1000ms, CHOICE, false, SceneScreen::GOAL_CHOICE},
    });
}

//  Blind presses are not busy: a minute of them still ends quiet; and only
//  an A mash after control presses blind.
UnitTestResult test_blind_press_keeps_quiet(Logger& logger){
    const UnitTestResult quiet = check_reads(arrival_config(), {
        {0ms, NOTHING, false, SceneScreen::DIALOG, false, true},
        {5000ms, NOTHING, true, SceneScreen::BLIND_PRESS},
        {6000ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {59000ms, NOTHING, true, SceneScreen::BLIND_PRESS},
        {60000ms, NOTHING, true, SceneScreen::QUIET_END},
    });
    if (quiet.result != UnitTestResult::PASSED){
        return quiet;
    }
    SceneMachineConfig to_choice = arrival_config();
    to_choice.goal = SceneMashGoal::CHOICE;
    to_choice.text_button = BUTTON_B;
    return check_reads(to_choice, {
        {0ms, NOTHING, false, SceneScreen::DIALOG, false, true},
        {30000ms, NOTHING, true, SceneScreen::SCENE_STILL},
    });
}

//  Without a quiet time only the party bar ends an overworld scene: a
//  still screen waits until the machine's timeout.
UnitTestResult test_no_quiet_never_ends_still(Logger&){
    SceneMachineConfig config;
    return check_reads(config, {
        {0ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {300000ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {300100ms, BLACK, true, SceneScreen::FADE_BLACK},
    });
}

//  Pancham cornered: B through the text and the scene, done at the choice.
UnitTestResult test_text_to_choice(Logger&){
    SceneMachineConfig config;
    config.goal = SceneMashGoal::CHOICE;
    config.text_button = BUTTON_B;
    return check_reads(config, {
        {0ms, NOTHING, false, SceneScreen::SCENE_MOVING},
        {1000ms, DIALOG, false, SceneScreen::DIALOG},
        {2000ms, PARTY_BAR, false, SceneScreen::SCENE_MOVING},
        {3000ms, CHOICE, false, SceneScreen::GOAL_CHOICE},
    });
}

//  What each state presses.
UnitTestResult test_state_actions(Logger&){
    SceneMachineConfig a_mash = opening_config(0ms);
    SceneMachineConfig b_to_choice;
    b_to_choice.goal = SceneMashGoal::CHOICE;
    b_to_choice.text_button = BUTTON_B;
    SceneMachineConfig to_battle;
    to_battle.goal = SceneMashGoal::BATTLE;
    struct Case{
        SceneScreen screen;
        const SceneMachineConfig& config;
        SceneMashAction expected;
    };
    const Case cases[] = {
        {SceneScreen::DIALOG, a_mash, SceneMashAction::PRESS_TEXT},
        {SceneScreen::CHOICE, a_mash, SceneMashAction::PRESS_A},
        {SceneScreen::FADE_WHITE, a_mash, SceneMashAction::WAIT},
        {SceneScreen::FADE_BLACK, b_to_choice, SceneMashAction::WAIT},
        {SceneScreen::PARTY_BAR, a_mash, SceneMashAction::WAIT},
        {SceneScreen::SCENE_STILL, a_mash, SceneMashAction::WAIT},
        {SceneScreen::SCENE_MOVING, b_to_choice, SceneMashAction::PRESS_B},
        {SceneScreen::SCENE_MOVING, to_battle, SceneMashAction::PRESS_A},
        {SceneScreen::CAN_MOVE, a_mash, SceneMashAction::DONE},
        {SceneScreen::HINT_SHOWING, a_mash, SceneMashAction::WAIT},
        {SceneScreen::CONTROLS_HINT, a_mash, SceneMashAction::DONE},
        {SceneScreen::QUIET_END, a_mash, SceneMashAction::DONE},
        {SceneScreen::BLIND_PRESS, a_mash, SceneMashAction::PRESS_A},
        {SceneScreen::MISSION_COMPLETE, a_mash, SceneMashAction::PRESS_A},
        {SceneScreen::MISSION_COMPLETE, b_to_choice, SceneMashAction::PRESS_A},
        {SceneScreen::GOAL_CHOICE, b_to_choice, SceneMashAction::DONE},
        {SceneScreen::GOAL_BATTLE, to_battle, SceneMashAction::DONE},
    };
    for (const Case& item : cases){
        const SceneMashAction got = scene_screen_action(item.screen, item.config);
        if (got != item.expected){
            return std::format("{}: expected action {}, got {}.",
                scene_screen_name(item.screen), (int)item.expected, (int)got);
        }
    }
    return true;
}

//  A battle goal presses A through cards and stops at the HUD.
UnitTestResult test_to_battle(Logger&){
    SceneMachineConfig config;
    config.goal = SceneMashGoal::BATTLE;
    return check_reads(config, {
        {0ms, DIALOG, false, SceneScreen::DIALOG},
        {1000ms, NOTHING, true, SceneScreen::SCENE_STILL},
        {2000ms, BATTLE, false, SceneScreen::GOAL_BATTLE},
    });
}


struct SceneMachineInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct SceneMachineExpectation{
};

struct SceneMachineCase{
    SceneMachineInput input;
    SceneMachineExpectation expectation;
};

class Test_AutoStorySceneMachine : public UnitTest{
public:
    explicit Test_AutoStorySceneMachine(SceneMachineCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStorySceneMachine - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return m_case.input.body(logger);
    }

private:
    SceneMachineCase m_case;
};

} // namespace


const SceneMachineCase SCENE_MACHINE_CASES[] = {
    {
        .input = {"quiet end waits for the scene's minimum length", test_quiet_waits_for_min_length},
        .expectation = {},
    },
    {
        .input = {"a box or a fade restarts the quiet time", test_quiet_restarts_on_busy},
        .expectation = {},
    },
    {
        .input = {"the party bar held a second ends the scene", test_party_bar_held},
        .expectation = {},
    },
    {
        .input = {"the controls hint held a second ends the scene", test_controls_hint_held},
        .expectation = {},
    },
    {
        .input = {"the ad's subtitles: A at the chevron, blind A, none at the hint", test_ad_subtitles},
        .expectation = {},
    },
    {
        .input = {"the mission banner on two looks ends the scene", test_mission_banner_ends},
        .expectation = {},
    },
    {
        .input = {"the mission complete card: A, busy, wins over the banner", test_mission_complete_presses_a},
        .expectation = {},
    },
    {
        .input = {"the mission banner does not end a mash to a choice", test_mission_banner_only_overworld},
        .expectation = {},
    },
    {
        .input = {"blind presses keep the quiet end, and only in an A mash", test_blind_press_keeps_quiet},
        .expectation = {},
    },
    {
        .input = {"without a quiet time a still screen never ends it", test_no_quiet_never_ends_still},
        .expectation = {},
    },
    {
        .input = {"B through the text to the choice", test_text_to_choice},
        .expectation = {},
    },
    {
        .input = {"A through the cards to the battle HUD", test_to_battle},
        .expectation = {},
    },
    {
        .input = {"what each state presses", test_state_actions},
        .expectation = {},
    },
};

void add_tests_AutoStorySceneMachine(UnitTestDatabase& database){
    for (const SceneMachineCase& item : SCENE_MACHINE_CASES){
        database.add<Test_AutoStorySceneMachine>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
