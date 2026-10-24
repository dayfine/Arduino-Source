#include <array>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonSV_MenuNavigation.h"
#include "PokemonSV_WorldNavigation.h"
#include "PokemonSV_RegionTravel.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

bool blueberry_entrance_attempt(const ProgramInfo& info, VideoStream& stream,
    ProControllerContext& context, Milliseconds adjustment
){
    pbf_mash_button(context, BUTTON_B, 800ms);
    open_map_from_overworld(info, stream, context);
    pbf_press_button(context, BUTTON_L, 400ms, 2400ms);
    pbf_move_left_joystick(context, {+1, -1}, 5000ms, 400ms);
    pbf_move_left_joystick(context, {-0.5, +0.5}, 840ms + adjustment, 400ms);
    return fly_to_overworld_from_map(info, stream, context, true);
}

}

void fly_from_paldea_to_blueberry_entrance(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    // Preserve the material farmer's search range and attempt order.
    const std::array<Milliseconds, 11> adjustments{
        0ms, 0ms, 32ms, -32ms, 64ms, -64ms, 128ms, -128ms, 0ms, 0ms, 0ms
    };
    for (const auto adjustment : adjustments){
        if (blueberry_entrance_attempt(info, stream, context, adjustment)){
            return;
        }
        stream.log("Failed to fly to Blueberry academy.");
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT, "Failed to fly to Blueberry academy after 11 attempts.", stream
    );
}

}
}
}
