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
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryConfirmChoicesReader.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryIntroScreens.h"
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

//  Fails, having pressed nothing, unless "Confirm Choices" shows the name
//  and the language of `choices`.
void check_profile(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    context.wait_for_all_requests();
    const VideoSnapshot frame = console.video().snapshot();
    if (!frame){
        return;
    }
    const std::string name = read_confirm_choices_name(frame);
    if (name != choices.name){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("AutoStory intro: Confirm Choices shows the name \"{}\", not \"{}\". Nothing was confirmed.", name, choices.name),
            console
        );
    }
    const NewGameLanguageInfo& info = new_game_language_info(choices.language);
    if (!OCR::ocr_language_available(info.ocr_language)){
        console.log(std::format("AutoStory intro: Confirm Choices: no OCR for {}; its language is not checked.", info.tag));
        return;
    }
    const std::string language = read_confirm_choices_language(frame, info.ocr_language);
    if (language.find(info.confirm_text) == std::string::npos){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("AutoStory intro: Confirm Choices shows the language \"{}\", not {}. Nothing was confirmed.", language, info.tag),
            console
        );
    }
    console.log(std::format("AutoStory intro: Confirm Choices shows \"{}\" in {}.", name, info.tag));
}

} // namespace


size_t require_confirm_choices(ConsoleHandle& console, ProControllerContext& context){
    const std::optional<size_t> cursor = read_confirm_choices_now(console, context, /*window=*/2000ms, "confirm-choices");
    if (!cursor){
        throw UserSetupError(
            console,
            "AutoStory: 01.00's confirm step needs the new game's \"Confirm Choices\" screen, after the language, look "
            "and name. It is not on screen; nothing was pressed."
        );
    }
    console.log(std::format("AutoStory intro: Confirm Choices on screen, row {} highlighted.", *cursor));
    return *cursor;
}

void confirm_new_game_choices(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    std::optional<size_t> cursor = require_confirm_choices(console, context);
    check_profile(console, context, choices);
    for (size_t press = 0; press < 6 && cursor; press++){
        if (*cursor == CONFIRM_CHOICES_READY_ROW){
            console.log("AutoStory intro: Confirm Choices: A on \"Ready!\".");
            pbf_press_button(context, BUTTON_A, /*hold=*/160ms, /*release=*/2000ms);
            context.wait_for_all_requests();
            return;
        }
        console.log("AutoStory intro: Confirm Choices: DOWN.");
        pbf_press_dpad(context, DPAD_DOWN, /*hold=*/160ms, /*release=*/500ms);
        cursor = read_confirm_choices_now(console, context, /*window=*/1000ms, "confirm-choices-moved");
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
