/*  Destination Marker Navigation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#include <cmath>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonSV/Inference/Map/PokemonSV_DestinationMarkerDetector.h"
#include "PokemonSV_MenuNavigation.h"
#include "PokemonSV_MarkerNavigation.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
DestinationMarkerState read_destination_marker(const ImageViewRGB32& screen){
    DestinationMarkerDetector arrival(COLOR_CYAN, {0.717, 0.165, 0.03, 0.061}, false);
    if (arrival.detect(screen)){ return DestinationMarkerState::ARRIVED; }
    DestinationMarkerDetector minimap(COLOR_BLUE, {0.815, 0.645, 0.180, 0.320}, true);
    return minimap.detect(screen) ? DestinationMarkerState::PRESENT : DestinationMarkerState::MISSING;
}
bool destination_marker_ahead(const ImageViewRGB32& screen){
    DestinationMarkerDetector minimap(COLOR_BLUE, {0.815, 0.645, 0.180, 0.320}, true);
    auto hits = minimap.detect_all(screen);
    const auto yellow = minimap.detect_all_yellow(screen);
    hits.insert(hits.end(), yellow.begin(), yellow.end());
    for (const auto& box : hits){
        const double x = box.x + box.width / 2;
        const double y = box.y + box.height / 2;
        // Player is at the minimap center. Require a marker in the forward cone before a pulse.
        if (std::abs(x - 0.906) <= 0.025 && y < 0.815){ return true; }
    }
    return false;
}
void realign_to_existing_destination(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    // Existing AutoStory old-marker behavior. Its heading effect still needs route-specific field validation.
    open_map_from_overworld(info, stream, context, false);
    leave_phone_to_overworld(info, stream, context);
    pbf_press_button(context, BUTTON_L, 160ms, 840ms);
    context.wait_for_all_requests();
}
}
}
}
