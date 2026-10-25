#ifndef PokemonAutomation_PokemonSV_IsolatedHuntTravel_H
#define PokemonAutomation_PokemonSV_IsolatedHuntTravel_H
#include "CommonFramework/Language.h"
#include "CommonFramework/Notifications/ProgramInfo.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonSV_IsolatedHuntPlan.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
bool travel_to_isolated_hunt(const ProgramInfo& info, VideoStream& stream,
    ProControllerContext& context, IsolatedHuntLocation location, bool from_zero_gate, Language language);
void approach_isolated_patch(VideoStream& stream, ProControllerContext& context, IsolatedHuntLocation location);
bool return_from_isolated_hunt(const ProgramInfo& info, VideoStream& stream,
    ProControllerContext& context, IsolatedHuntLocation location);
}
}
}
#endif
