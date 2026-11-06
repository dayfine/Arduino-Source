/*  SID Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonFRLG_RngCalibration.h"
#include "PokemonFRLG_SidSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


SidTiming sid_timing(Language language, uint32_t target_advances){
    uint64_t FINAL_TEXT_FRAMES;
    switch (language){
    case Language::English:
        FINAL_TEXT_FRAMES = 249;
        break;
    case Language::Japanese:
        FINAL_TEXT_FRAMES = 194;
        break;
    case Language::Italian:
        FINAL_TEXT_FRAMES = 236;
        break;
    case Language::French:
        FINAL_TEXT_FRAMES = 205;
        break;
    case Language::German:
        FINAL_TEXT_FRAMES = 208;
        break;
    case Language::Spanish:
        FINAL_TEXT_FRAMES = 202;
        break;
    default:
        FINAL_TEXT_FRAMES = 249;
    }

    static const int64_t FIXED_ADVANCES_OFFSET = 7; // determined empirically. Probably not console/setup dependent

    bool extra_press_at_end = (
        language == Language::German ||
        language == Language::Japanese ||
        language == Language::Spanish ||
        language == Language::French
    );

    const uint64_t SID_DELAY = uint64_t((target_advances - 2*FINAL_TEXT_FRAMES + FIXED_ADVANCES_OFFSET) * FRLG_FRAME_DURATION / 2); // advances pass 2 by 2
    return SidTiming{.delay_ms = SID_DELAY, .extra_press_at_end = extra_press_at_end};
}

void set_sid_from_name_screen(ProControllerContext& context, const SidTiming& timing){
    // this is performed blind to try to maximize consistency of timing
    // ensure the OK button is selected
    pbf_press_button(context, BUTTON_PLUS, 200ms, 300ms);
    // confirm name (SID delay starts now)
    pbf_press_button(context, BUTTON_A, 200ms, 1800ms);
    // advance to "...about to unfold!", picking the default rival name
    pbf_press_button(context, BUTTON_A, 200ms, 2300ms);
    pbf_press_button(context, BUTTON_A, 200ms, 1050ms);
    pbf_press_button(context, BUTTON_A, 200ms, 1550ms);
    pbf_move_left_joystick(context, {0,-1}, 200ms, 300ms);
    pbf_press_button(context, BUTTON_A, 200ms, 1050ms);
    pbf_press_button(context, BUTTON_A, 200ms, 1550ms);
    pbf_press_button(context, BUTTON_A, 200ms, 2300ms);

    // 2000 + 2500 + 1250 + 1750 + 500 + 1250 + 1750 + 2500 + 200 ms
    Milliseconds delay(timing.delay_ms > 13700
        ? timing.delay_ms - 13700
        : 0
    );

    if (timing.extra_press_at_end){
        pbf_press_button(context, BUTTON_A, 200ms, 1300ms);
        delay = delay > 1500ms ? delay - 1500ms : 0ms;
    }

    pbf_press_button(context, BUTTON_A, 200ms, delay);

    // finish dialogue and lock in SID
    pbf_press_button(context, BUTTON_A, 200ms, 0ms);
    context.wait_for_all_requests();
}

void finish_intro_animations(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    BlackScreenWatcher black_screen;
    context.wait_for_all_requests();
    int ret = wait_until(
        env.console, context, 10s,
        { black_screen }
    );

    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT, "SidHelper(): black screen not detected within 10 seconds of setting SID", env.console
        );
    }

    BlackScreenOverWatcher black_screen_over;
    context.wait_for_all_requests();
    int ret2 = wait_until(
        env.console, context, 10s,
        { black_screen_over }
    );

    if (ret2 < 0){
        env.log("finish_intro_animations(): end of black screen not detected within 10 seconds. Continuing anyway...");
        // if there's a problem, an error will be thrown when trying to navigate to the trainer card
    }

    pbf_wait(context, 2000ms);
    context.wait_for_all_requests();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
