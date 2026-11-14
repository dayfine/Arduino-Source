/*  PokemonLZA AutoStory - Dialogs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include "Common/Cpp/Color.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_OverworldPartySelectionDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "PokemonLZA_AutoStoryDialogs.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


void press_until_overworld(
    ConsoleHandle& console, ProControllerContext& context,
    const std::function<void(ProControllerContext&)>& presses,
    Milliseconds timeout, std::string_view failure
){
    OverworldPartySelectionWatcher overworld(COLOR_WHITE, &console.overlay(), OVERWORLD_HOLD);
    context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        console, context,
        [&presses, timeout](ProControllerContext& context){
            const WallClock deadline = current_time() + timeout;
            while (current_time() < deadline){
                presses(context);
                context.wait_for_all_requests();
            }
        },
        {overworld}
    );
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::string(failure),
            console
        );
    }
}

void mash_to_overworld(ConsoleHandle& console, ProControllerContext& context, Button button, Milliseconds timeout){
    press_until_overworld(
        console, context,
        [button, timeout](ProControllerContext& context){
            pbf_mash_button(context, button, timeout);
        },
        timeout, std::format("mash_to_overworld(): no overworld after {} ms.", timeout.count())
    );
}

ScreensPassed press_a_until_quiet(
    ConsoleHandle& console, ProControllerContext& context,
    StoryDialogKinds kinds, Milliseconds quiet, Milliseconds timeout
){
    StoryDialogDetector any_dialog(kinds);
    ScreensPassed passed;
    WallClock last_busy = current_time();
    const WallClock deadline = last_busy + timeout;
    while (current_time() - last_busy < quiet){
        if (current_time() >= deadline){
            passed.timed_out = true;
            return passed;
        }
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        const bool card_now = screen && tutorial_card_shows(screen);
        if (card_now || (screen && any_dialog.detect(screen))){
            passed.card = passed.card || card_now;
            passed.text = passed.text || !card_now;
            last_busy = current_time();
            pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        }else{
            context.wait_for(250ms);
        }
    }
    return passed;
}

void talk_to_facing(ConsoleHandle& console, ProControllerContext& context){
    StoryDialogWatcher dialog;
    context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_press_button(context, BUTTON_A, 160ms, 5000ms);
        },
        {dialog}
    );
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "talk_to_facing(): A opened no dialogue.",
            console
        );
    }
}


const ImageFloatBox& dialog_choice_box(){
    static const ImageFloatBox box{0.654308, 0.481553, 0.295529, 0.312621};
    return box;
}

namespace{

//  The selection arrow's top among the choices, once it has held still;
//  none if it does not show within `timeout`.
std::optional<double> wait_for_choice_arrow(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout){
    SelectionArrowWatcher arrow(COLOR_YELLOW, &console.overlay(), SelectionArrowType::RIGHT, dialog_choice_box());
    context.wait_for_all_requests();
    if (wait_until(console, context, timeout, {arrow}) < 0){
        return std::nullopt;
    }
    return arrow.last_detected().y;
}

} // namespace

//  TODO: unify with get_current_selector_index()
//  (PokemonLZA_FastTravelNavigation.h) - the dialog choices' row spacing is
//  not measured, so this counts rows down from the top.
void choose_dialog_option(ConsoleHandle& console, ProControllerContext& context, size_t index){
    //  Some questions show their choices only after A closes the question's
    //  text: the Research Lab elevator's "which floor?" (console B,
    //  2026-11-10). A press comes only after 3 s without choices, so it
    //  cannot pick the first choice the moment the choices appear.
    std::optional<double> row = wait_for_choice_arrow(console, context, 3s);
    for (int press = 0; !row && press < 3; press++){
        pbf_press_button(context, BUTTON_A, 160ms, 240ms);
        row = wait_for_choice_arrow(console, context, 3s);
    }
    if (!row){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "choose_dialog_option(): no choices on screen.",
            console
        );
    }
    for (size_t moved = 0; moved < index; moved++){
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 240ms);
        const std::optional<double> next = wait_for_choice_arrow(console, context, 2s);
        if (!next || *next <= *row){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::format("choose_dialog_option(): the arrow did not move down to choice {}.", moved + 1),
                console
            );
        }
        row = next;
    }
    pbf_press_button(context, BUTTON_A, 160ms, 240ms);
    context.wait_for_all_requests();
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
