/*  PokemonLZA AutoStory - Objective marker reader
 *
 *  From: https://github.com/PokemonAutomation/
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


//  The main mission's objective marker in the overworld: an orange hexagon
//  "!" with the distance above it in white digits.
struct ObjectiveMarker{
    //  The hexagon's middle, 0 (left edge) to 1 (right edge).
    double x = 0;
    //  The digits read; -1 when they did not read.
    int distance = -1;
};

//  The marker on `screen`, if it shows (the minimap's own marker aside).
std::optional<ObjectiveMarker> read_objective_marker(Logger& logger, const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
