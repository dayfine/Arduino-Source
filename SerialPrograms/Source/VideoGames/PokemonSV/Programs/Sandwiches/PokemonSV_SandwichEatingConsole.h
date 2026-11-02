#ifndef PokemonAutomation_PokemonSV_SandwichEatingConsole_H
#define PokemonAutomation_PokemonSV_SandwichEatingConsole_H

#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

bool wait_for_sandwich_picnic(VideoStream& stream, ProControllerContext& context);

}
}
}
#endif
