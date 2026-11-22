/*  BDSP Starter Navigation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_EndBattleDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP_StarterNavigation.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


const Milliseconds AFTER_BRIEFCASE = 4000ms;
const Milliseconds PRE_BRIEFCASE_DURATION = 21000ms;
const Milliseconds PROMPT_ANIMATION = 1000ms;
const Milliseconds SCHEDULING_SLACK = 100ms;


uint8_t starter_cursor_steps(BdspStarter starter){
    switch (starter){
    case BdspStarter::Turtwig:  return 0;
    case BdspStarter::Chimchar: return 1;
    case BdspStarter::Piplup:   return 2;
    }
    return 0;
}

std::string_view starter_slug(BdspStarter starter){
    switch (starter){
    case BdspStarter::Turtwig:  return "turtwig";
    case BdspStarter::Chimchar: return "chimchar";
    case BdspStarter::Piplup:   return "piplup";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown starter.");
}

//  behind the house to the north
void navigate_bedroom_to_skip_spot(Logger& logger, ProControllerContext& context){
    logger.log("Leaving the bedroom for the spot where advances pass quickly.");
    pbf_mash_button(context, BUTTON_B, 1000ms); // exit dialogue with the Switch
    pbf_wait(context, 1000ms);
    pbf_press_dpad(context, DPAD_RIGHT, 1200ms, 1500ms);
    pbf_press_dpad(context, DPAD_DOWN, 2300ms, 500ms);
    pbf_press_dpad(context, DPAD_LEFT, 1100ms, 1000ms);
    pbf_press_dpad(context, DPAD_DOWN, 750ms, 4000ms);
    pbf_press_dpad(context, DPAD_LEFT, 850ms, 1000ms);
    pbf_press_dpad(context, DPAD_UP, 4100ms, 1000ms);
    pbf_press_dpad(context, DPAD_RIGHT, 950ms, 500ms);
    context.wait_for_all_requests();
}


void navigate_skip_spot_to_lakefront(Logger& logger, ProControllerContext& context){
    logger.log("Walking from the skip spot to the lakefront.");
    pbf_press_dpad(context, DPAD_LEFT, 1350ms, 500ms);
    pbf_press_dpad(context, DPAD_UP, 5000ms, 0ms);

    //  Clearing the dialogue with Barry
    pbf_mash_button(context, BUTTON_B, 5000ms);
    pbf_wait(context, 1000ms);

    pbf_press_dpad(context, DPAD_UP, 600ms, 1000ms);
    pbf_press_dpad(context, DPAD_LEFT, 8500ms, 1000ms);
    pbf_press_dpad(context, DPAD_UP, 1950ms, 1000ms);
    context.wait_for_all_requests();
}


void navigate_to_lake_blinks(Logger& logger, ProControllerContext& context){
    logger.log("Walking into the lake, then advancing to the lake blink position.");
    pbf_move_left_joystick(context, {0.0, 1.0}, 1600ms, 0ms);

    pbf_wait(context, 1500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 4000ms);
    pbf_press_button(context, BUTTON_A, 100ms, 2000ms);
    pbf_press_button(context, BUTTON_A, 100ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 2500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 5500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 4500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 0ms);
    context.wait_for_all_requests();
}

void navigate_to_pre_briefcase(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Advancing to the pre-briefcase position.");

    //  Dismiss the lake-blink line; Barry turns north to spot the briefcase
    //  (~2.5-3.5s with no dialogue box) before the pre-walk conversation begins.
    pbf_wait(context, 1500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 1000ms);
    context.wait_for_all_requests();
    {
        ShortDialogWatcher first_dialog;
        wait_until(env.console, context, 5000ms, {{first_dialog}});
        pbf_wait(context, 500ms);
        context.wait_for_all_requests();
    }

    //  Advance pre-walk dialogue boxes until the walk into the tall grass starts
    //  (dialog box stays absent > 1500ms). Different game languages split this
    //  conversation into different box counts (e.g. 4 in English, 5 in Japanese).
    bool entered_walk = false;
    for (size_t i = 0; i < 10; i++){
        pbf_press_button(context, BUTTON_A, 100ms, 600ms);
        context.wait_for_all_requests();

        ShortDialogWatcher dialog;
        if (wait_until(env.console, context, 1500ms, {{dialog}}) < 0){
            entered_walk = true;
            env.log(
                "Tall-grass walk detected after " + std::to_string(i + 1)
                + " pre-walk dialogue press(es)."
            );
            break;
        }
    }

    //  Wait for the tall-grass walk + briefcase zoom to open Briefcase Box 1,
    //  then advance two boxes to Briefcase Box 3 (the line right before Starly).
    if (entered_walk){
        ShortDialogWatcher briefcase_dialog;
        wait_until(env.console, context, 8000ms, {{briefcase_dialog}});
        pbf_wait(context, 500ms);
    }else{
        env.log(
            "Tall-grass walk not detected via dialog gap; falling back to timed wait.",
            COLOR_ORANGE
        );
        pbf_wait(context, 4000ms);
    }
    pbf_press_button(context, BUTTON_A, 100ms, 1800ms);
    pbf_press_button(context, BUTTON_A, 100ms, 0ms);
    context.wait_for_all_requests();
}


const Milliseconds BRIEFCASE_PRESS_TO_STARLY_READY =
    80ms + AFTER_BRIEFCASE + 100ms;
const Milliseconds SELECT_PRESS_TO_CONFIRM_READY =
    80ms + PROMPT_ANIMATION + 100ms + 200ms;

static Milliseconds starly_press_to_select_ready(BdspStarter starter){
    return 80ms + 5500ms + 100ms + 1200ms
        + starter_cursor_steps(starter) * (100ms + 600ms);
}


namespace{

struct StarterSequenceWaits{
    Milliseconds before_starly = 0ms;
    Milliseconds before_select = 0ms;
    Milliseconds before_confirm = 0ms;
};

Milliseconds seconds_to_ms(double seconds){
    return std::chrono::duration_cast<Milliseconds>(std::chrono::duration<double>(seconds));
}

bool compute_starter_waits(
    BdspStarter starter,
    double starly_seconds,
    double select_seconds,
    double confirm_seconds,
    StarterSequenceWaits& waits,
    std::string& failure_reason
){
    //  Where the run stands after each fixed stretch,
    //  measured from the moment the briefcase press is completed.
    const Milliseconds starly_at = seconds_to_ms(starly_seconds);
    const Milliseconds select_at = seconds_to_ms(select_seconds);
    const Milliseconds confirm_at = seconds_to_ms(confirm_seconds);
    const Milliseconds after_dialog = BRIEFCASE_PRESS_TO_STARLY_READY;
    const Milliseconds after_starly = starly_at + starly_press_to_select_ready(starter);
    const Milliseconds after_select = select_at + SELECT_PRESS_TO_CONFIRM_READY;

    auto too_tight = [&](std::string_view what, Milliseconds needed, Milliseconds have){
        failure_reason = std::format(
            "{} is scheduled {} ms before the navigation ahead of it can finish",
            what, (needed - have).count()
        );
        return false;
    };
    if (starly_at < after_dialog){
        return too_tight("the Starly press", after_dialog, starly_at);
    }
    if (select_at < after_starly){
        return too_tight("the selecting press", after_starly, select_at);
    }
    if (confirm_at < after_select){
        return too_tight("the confirming press", after_select, confirm_at);
    }
    waits.before_starly = starly_at - after_dialog;
    waits.before_select = select_at - after_starly;
    waits.before_confirm = confirm_at - after_select;
    return true;
}

} // namespace

//  The whole blind button press sequence
bool issue_starter_sequence(
    ProControllerContext& context,
    BdspStarter starter,
    double starly_seconds,
    double select_seconds,
    double confirm_seconds,
    std::string& failure_reason
){
    StarterSequenceWaits waits;
    if (!compute_starter_waits(starter, starly_seconds, select_seconds, confirm_seconds, waits, failure_reason)){
        return false;
    }

    //  The briefcase press and past the briefcase dialogue
    pbf_press_button(context, BUTTON_A, 80ms, 0ms);
    pbf_wait(context, AFTER_BRIEFCASE);
    pbf_press_button(context, BUTTON_A, 100ms, 0ms);

    //  The Starly press, then open the briefcase and hover over the chosen ball
    pbf_wait(context, waits.before_starly);
    pbf_press_button(context, BUTTON_A, 80ms, 0ms);
    pbf_wait(context, 5500ms);
    pbf_press_button(context, BUTTON_A, 100ms, 0ms);
    pbf_wait(context, 1200ms);
    for (uint8_t c = 0; c < starter_cursor_steps(starter); c++){
        pbf_press_dpad(context, DPAD_RIGHT, 100ms, 600ms);
    }

    //  The selecting press, cursor onto "Yes", confirming press, then battle start
    pbf_wait(context, waits.before_select);
    pbf_press_button(context, BUTTON_A, 80ms, 0ms);
    pbf_wait(context, PROMPT_ANIMATION);
    pbf_press_dpad(context, DPAD_UP, 100ms, 200ms);
    pbf_wait(context, waits.before_confirm);
    pbf_press_button(context, BUTTON_A, 80ms, 0ms);
    pbf_wait(context, 3000ms);
    pbf_press_button(context, BUTTON_A, 100ms, 0ms);

    context.wait_for_all_requests();
    return true;
}


//  Where the blind run leaves off. Feedback-driven, since nothing past the
//  confirming press is timed against the RNG.
bool clear_starter_battle(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Mashing A through the battle.");
    EndBattleWatcher battle_over;
    int ret = run_until<ProControllerContext>(
        env.console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_A, 180s);
        },
        {{battle_over}}
    );
    if (ret < 0){
        env.log("The battle never ended.", COLOR_RED);
        return false;
    }
    env.log("Battle over.");
    return true;
}


double seconds_to_move_to_starter(BdspStarter starter){
    return std::chrono::duration_cast<std::chrono::duration<double>>(
        starly_press_to_select_ready(starter) - 80ms
    ).count();
}

//  Kept as short as the prompt animation allows to avoid extra blinks from the starter.
StarterSceneDurations StarterSceneDurations::measured(){
    auto to_seconds = [](Milliseconds duration){
        return std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();
    };
    return StarterSceneDurations{
        .briefcase_to_starly_ready = to_seconds(BRIEFCASE_PRESS_TO_STARLY_READY),
        .select_to_confirm = to_seconds(SELECT_PRESS_TO_CONFIRM_READY + SCHEDULING_SLACK),
        .to_pre_briefcase = to_seconds(PRE_BRIEFCASE_DURATION),
    };
}




} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
