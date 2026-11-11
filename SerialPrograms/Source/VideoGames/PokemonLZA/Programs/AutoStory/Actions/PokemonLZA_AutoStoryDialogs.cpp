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

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


void mash_to_overworld(ConsoleHandle& console, ProControllerContext& context, Button button, Milliseconds timeout){
    //  The party list shows between lines of a scene too; a second of it is
    //  the overworld.
    OverworldPartySelectionWatcher overworld(COLOR_WHITE, &console.overlay(), 1000ms);
    context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        console, context,
        [button, timeout](ProControllerContext& context){
            pbf_mash_button(context, button, timeout);
        },
        {overworld}
    );
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("mash_to_overworld(): no overworld after {} ms.", timeout.count()),
            console
        );
    }
}

void talk_to_facing(ConsoleHandle& console, ProControllerContext& context){
    FlatWhiteDialogWatcher white(COLOR_RED, &console.overlay());
    BlueDialogWatcher blue(COLOR_BLUE, &console.overlay());
    TealDialogWatcher teal(COLOR_GREEN, &console.overlay());
    context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_press_button(context, BUTTON_A, 160ms, 5000ms);
        },
        {white, blue, teal}
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

void choose_dialog_option(ConsoleHandle& console, ProControllerContext& context, size_t index){
    std::optional<double> row = wait_for_choice_arrow(console, context, 10s);
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
