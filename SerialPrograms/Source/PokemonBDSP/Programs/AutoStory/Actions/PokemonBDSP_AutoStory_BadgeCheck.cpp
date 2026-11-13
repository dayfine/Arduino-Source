/*  BDSP AutoStory - Badge Check
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_TrainerCardReader.h"
#include "PokemonBDSP_AutoStory_Actions.h"
#include "PokemonBDSP_AutoStory_BadgeCheck.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


void verify_badge(
    VideoStream& stream, ProControllerContext& context,
    Pokemon::StoryCheckLevel level, size_t index, std::string_view badge, int menu_icons
){
    if (!Pokemon::badge_check_wanted(level)){
        return;
    }
    open_menu(stream, context, MenuCursorPosition::CARD, menu_icons);
    //  The card slides in.
    context.wait_for(2000ms);
    context.wait_for_all_requests();
    const VideoSnapshot card = stream.video().snapshot();
    if (!card){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Badge check: no frame of the Trainer Card.", stream);
    }
    //  Kept for the reader's fixtures (Pokemon::BadgeReading).
    const std::string folder = SCREENSHOTS_PATH() + "AutoStory/cards/";
    Filesystem::create_directories(Filesystem::Path(folder));
    const std::string path = folder + now_to_filestring() + ".png";
    card.frame->save(path);
    stream.log("Trainer Card screenshot: " + path);

    const Pokemon::BadgeReading reading = read_trainer_card_badge(*card.frame, index);
    const std::string failure = Pokemon::badge_check_failure(reading, badge);
    if (!failure.empty()){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, failure, stream);
    }
    pbf_mash_button(context, BUTTON_B, 3000ms);
    context.wait_for_all_requests();
    stream.log(std::string("Badge check: the ") + std::string(badge) + " is on the Trainer Card.");
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
