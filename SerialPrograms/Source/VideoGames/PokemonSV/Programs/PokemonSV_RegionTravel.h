#ifndef PokemonAutomation_PokemonSV_RegionTravel_H
#define PokemonAutomation_PokemonSV_RegionTravel_H
#include "CommonFramework/Notifications/ProgramInfo.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
void fly_from_paldea_to_blueberry_entrance(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);
}
}
}
#endif
