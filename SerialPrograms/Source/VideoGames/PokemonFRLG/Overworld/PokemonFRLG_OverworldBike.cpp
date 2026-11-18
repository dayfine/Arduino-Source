/*  Overworld Bicycle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "PokemonFRLG_BikeSpinPresses.h"
#include "PokemonFRLG_OverworldBike.h"
#include "PokemonFRLG_PlayerFacingReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace OverworldWalkSteps{

using namespace MovementMechanics;


bool select_onto_bike(const BikeSelectCalls& calls){
    for (int press = 1; press <= 2; press++){
        calls.press_select();
        if (calls.riding()){
            return true;
        }
        if (calls.wait_to_read_again){
            calls.wait_to_read_again();
        }
        if (calls.riding()){
            return true;
        }
    }
    return false;
}

bool screen_shows_riding(const ImageViewRGB32& screen, PlayerGender player){
    if (StartMenuDetector(COLOR_RED).detect(screen) || SelectionDialogDetector(COLOR_RED).detect(screen)
        || WhiteDialogDetector(COLOR_RED).detect(screen)
    ){
        return false;
    }
    return !read_player_facing(screen, player).has_value();
}

void get_on_bike(ConsoleHandle& console, ProControllerContext& context, PlayerGender player){
    BikeSelectCalls calls;
    calls.press_select = [&]{
        pbf_press_button(context, BUTTON_MINUS, BIKE_SELECT_PRESS, BIKE_SELECT_SETTLE);
    };
    calls.riding = [&]{
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        const bool riding = screen && screen_shows_riding(*screen.frame, player);
        console.log(riding ? "Bike: riding." : "Bike: not riding after SELECT.", riding ? COLOR_BLACK : COLOR_ORANGE);
        return riding;
    };
    calls.wait_to_read_again = [&]{
        pbf_wait(context, BIKE_SELECT_SETTLE);
    };
    console.log("Bike: SELECT (MINUS) onto the Bicycle.");
    if (!select_onto_bike(calls)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "get_on_bike(): not on the Bicycle after SELECT twice. Is it registered?",
            console
        );
    }
}


} // namespace OverworldWalkSteps
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
