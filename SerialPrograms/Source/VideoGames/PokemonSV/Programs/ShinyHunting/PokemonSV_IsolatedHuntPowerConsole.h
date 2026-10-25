#ifndef PokemonAutomation_PokemonSV_IsolatedHuntPowerConsole_H
#define PokemonAutomation_PokemonSV_IsolatedHuntPowerConsole_H
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonSV_IsolatedHuntPlan.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
std::string read_isolated_encounter_power(const ImageViewRGB32& screen, Language language);
bool check_isolated_meal_power(VideoStream& stream, ProControllerContext& context,
    Language language, IsolatedEncounterType type, uint8_t level);
}
}
}
#endif
