#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_RegionTravel.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_Terarium.h"
#include "PokemonSV_IsolatedHuntTravel.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

bool seek_paldea_flight(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context,
    double x, double y, Milliseconds duration, FlyPoint kind
){
    open_map_from_overworld(info, stream, context);
    pbf_press_button(context, BUTTON_ZL, 160ms, 840ms);
    pbf_move_left_joystick(context, {x, y}, duration, 800ms);
    pbf_press_button(context, BUTTON_ZR, 160ms, 840ms);
    context.wait_for_all_requests();
    return fly_to_visible_closest_flypoint_cur_zoom_level(info, stream, context, kind);
}

bool refly_local_anchor(const ProgramInfo& info, VideoStream& stream,
    ProControllerContext& context, IsolatedHuntLocation location
){
    open_map_from_overworld(info, stream, context);
    context.wait_for_all_requests();
    const auto kind = location == IsolatedHuntLocation::CHARGESTONE_CAVE ? FlyPoint::FAST_TRAVEL : FlyPoint::POKECENTER;
    return fly_to_visible_closest_flypoint_cur_zoom_level(info, stream, context, kind);
}

}

bool travel_to_isolated_hunt(const ProgramInfo& info, VideoStream& stream,
    ProControllerContext& context, IsolatedHuntLocation location, bool from_zero_gate
){
    if (!from_zero_gate){
        return refly_local_anchor(info, stream, context, location);
    }
    if (location == IsolatedHuntLocation::CHARGESTONE_CAVE){
        fly_from_paldea_to_blueberry_entrance(info, stream, context);
        return_to_plaza(info, stream, context);
        central_to_chargestone(info, stream, context);
        return true;
    }
    // Experimental initial seek from Zero Gate, based on the relative Paldea map coordinates.
    // Icon inference selects the nearby center; its actual identity must be checked in the field.
    stream.log("Experimental flight: Zero Gate to West Province Area One Central.");
    return seek_paldea_flight(info, stream, context, -1, -0.47, 850ms, FlyPoint::POKECENTER);
}

void approach_isolated_patch(VideoStream& stream, ProControllerContext& context, IsolatedHuntLocation location){
    pbf_press_button(context, BUTTON_L, 160ms, 400ms);
    if (location == IsolatedHuntLocation::CHARGESTONE_CAVE){
        stream.log("Approach: existing Chargestone court; verify this is the Chespin Cave habitat.");
        pbf_move_left_joystick(context, {0, +1}, 2000ms, 400ms);
        pbf_move_left_joystick(context, {+1, 0}, 1440ms, 400ms);
        pbf_press_button(context, BUTTON_L, 160ms, 400ms);
    }else{
        stream.log("Approach: experimental clearing beside West Province One Central.");
        pbf_move_left_joystick(context, {0, -1}, 1200ms, 400ms);
        pbf_move_left_joystick(context, {-1, 0}, 1600ms, 400ms);
        pbf_press_button(context, BUTTON_L, 160ms, 400ms);
    }
    context.wait_for_all_requests();
}

bool return_from_isolated_hunt(const ProgramInfo& info, VideoStream& stream,
    ProControllerContext& context, IsolatedHuntLocation location
){
    if (location == IsolatedHuntLocation::CHARGESTONE_CAVE){
        open_map_from_overworld(info, stream, context);
        pbf_press_button(context, BUTTON_R, 400ms, 2400ms);
        pbf_press_button(context, BUTTON_ZL, 160ms, 840ms);
        pbf_move_left_joystick(context, {-1, 0}, 200ms, 800ms);
        pbf_press_button(context, BUTTON_ZR, 160ms, 840ms);
        context.wait_for_all_requests();
        return fly_to_visible_closest_flypoint_cur_zoom_level(info, stream, context, FlyPoint::FAST_TRAVEL);
    }
    return seek_paldea_flight(info, stream, context, +1, +0.47, 850ms, FlyPoint::FAST_TRAVEL);
}

}
}
}
