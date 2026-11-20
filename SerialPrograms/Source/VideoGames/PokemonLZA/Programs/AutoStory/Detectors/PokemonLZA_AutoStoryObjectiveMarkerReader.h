/*  PokemonLZA AutoStory - Objective marker reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Detects the main-mission objective marker and reads its distance label.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryObjectiveMarkerReader_H
#define PokemonAutomation_PokemonLZA_AutoStoryObjectiveMarkerReader_H

#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Logging/Logger.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Detected objective marker horizontal position (normalized 0..1) and optional positive distance.
struct ObjectiveMarker{
    double x = 0;
    std::optional<int> distance = std::nullopt;
};

//  Returns the on-screen main-mission objective marker (excluding the minimap), or std::nullopt.
std::optional<ObjectiveMarker> read_objective_marker(Logger& logger, const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
