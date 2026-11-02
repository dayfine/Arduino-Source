/*  Destination Marker Navigation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#ifndef PokemonAutomation_PokemonSV_MarkerNavigation_H
#define PokemonAutomation_PokemonSV_MarkerNavigation_H
#include "CommonFramework/Notifications/ProgramInfo.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
enum class DestinationMarkerState{ MISSING, PRESENT, ARRIVED };
DestinationMarkerState read_destination_marker(const ImageViewRGB32& screen);
// Conservative minimap bearing gate, not a terrain-clearance test.
bool destination_marker_ahead(const ImageViewRGB32& screen);
// Caller owns battle/refusal watchers and deadlines. This routine never flees a battle.
void realign_to_existing_destination(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);
}
}
}
#endif
