/*  PokemonLZA AutoStory - Confirm Choices
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryConfirmChoicesReader.h"
#include "PokemonLZA_AutoStoryConfirmChoices.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The "Confirm Choices" highlight on the screen now, reading a few frames
//  over `window` with no press. Nullopt if none of them is the screen.
//  `dump`: save the frame read to DebugDumps under that name.
std::optional<size_t> read_confirm_choices_now(
    ConsoleHandle& console, ProControllerContext& context, Milliseconds window, const std::string& dump
){
    const WallClock deadline = current_time() + window;
    while (true){
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        const std::optional<size_t> cursor = frame ? read_confirm_choices_cursor(frame) : std::nullopt;
        if (cursor || current_time() >= deadline){
            if (frame){
                dump_debug_image(console, "PokemonLZA/AutoStory/Intro", dump, frame);
            }
            return cursor;
        }
        context.wait_for(250ms);
    }
}

} // namespace


size_t require_confirm_choices(ConsoleHandle& console, ProControllerContext& context){
    const std::optional<size_t> cursor = read_confirm_choices_now(console, context, 2000ms, "confirm-choices");
    if (!cursor){
        throw UserSetupError(
            console,
            "AutoStory: 01.00 starts on the new game's \"Confirm Choices\" screen, with the language, look and name "
            "already chosen by hand. It is not on screen; nothing was pressed."
        );
    }
    console.log(std::format("AutoStory intro: Confirm Choices on screen, row {} highlighted.", *cursor));
    return *cursor;
}

void confirm_new_game_choices(ConsoleHandle& console, ProControllerContext& context){
    std::optional<size_t> cursor = require_confirm_choices(console, context);
    for (size_t press = 0; press < 6 && cursor; press++){
        if (*cursor == CONFIRM_CHOICES_READY_ROW){
            console.log("AutoStory intro: Confirm Choices: A on \"Ready!\".");
            pbf_press_button(context, BUTTON_A, 160ms, 2000ms);
            context.wait_for_all_requests();
            return;
        }
        console.log("AutoStory intro: Confirm Choices: DOWN.");
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 500ms);
        cursor = read_confirm_choices_now(console, context, 1000ms, "confirm-choices-moved");
        if (cursor){
            console.log(std::format("AutoStory intro: Confirm Choices: the highlight is on row {}.", *cursor));
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "AutoStory intro: the Confirm Choices highlight was not read on \"Ready!\".",
        console
    );
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
