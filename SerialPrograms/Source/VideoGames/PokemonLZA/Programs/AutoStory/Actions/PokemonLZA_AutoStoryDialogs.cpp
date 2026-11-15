/*  PokemonLZA AutoStory - Dialogs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include "Common/Cpp/Color.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_OverworldPartySelectionDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_FastTravelNavigation.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDialogChoiceBox.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
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

namespace{

//  Whether the selection arrow shows among the choices, held still, within
//  `timeout`.
bool choice_arrow_shows(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout){
    SelectionArrowWatcher arrow(COLOR_YELLOW, &console.overlay(), SelectionArrowType::RIGHT, dialog_choice_box());
    context.wait_for_all_requests();
    return wait_until(console, context, timeout, {arrow}) >= 0;
}

//  The choice row the selection arrow is on, counted from the top of
//  dialog_choice_rows_box(); none if it is not in the rows.
std::optional<size_t> read_choice_row(ConsoleHandle& console){
    const int row = get_current_selector_index(console, dialog_choice_rows_box(), DIALOG_CHOICE_ROW_SPACING);
    if (row < 0){
        return std::nullopt;
    }
    return static_cast<size_t>(row);
}

} // namespace

int dialog_choice_moves(size_t current, size_t target){
    return static_cast<int>(target) - static_cast<int>(current);
}

void choose_dialog_option(ConsoleHandle& console, ProControllerContext& context, size_t index){
    //  Some questions show their choices only after A closes the question's
    //  text: the Research Lab elevator's "which floor?" (console B,
    //  2026-11-10). A press comes only after 3 s without choices, so it
    //  cannot pick the first choice the moment the choices appear.
    bool shows = choice_arrow_shows(console, context, 3s);
    for (int press = 0; !shows && press < 3; press++){
        pbf_press_button(context, BUTTON_A, 160ms, 240ms);
        shows = choice_arrow_shows(console, context, 3s);
    }
    if (!shows){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "choose_dialog_option(): no choices on screen.",
            console
        );
    }
    const std::optional<size_t> row = read_choice_row(console);
    if (!row){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "choose_dialog_option(): the arrow is outside the choice rows (dialog_choice_rows_box()).",
            console
        );
    }
    const int moves = dialog_choice_moves(*row, index);
    const DpadPosition direction = moves > 0 ? DPAD_DOWN : DPAD_UP;
    for (int moved = 0; moved < std::abs(moves); moved++){
        pbf_press_dpad(context, direction, 160ms, 240ms);
    }
    if (moves != 0){
        choice_arrow_shows(console, context, 2s);
        const std::optional<size_t> now = read_choice_row(console);
        if (now != index){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::format(
                    "choose_dialog_option(): the arrow is on row {}, not {}, after {} presses from row {}.",
                    now ? std::to_string(*now) : "none", index, moves, *row
                ),
                console
            );
        }
    }
    pbf_press_button(context, BUTTON_A, 160ms, 240ms);
    context.wait_for_all_requests();
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
