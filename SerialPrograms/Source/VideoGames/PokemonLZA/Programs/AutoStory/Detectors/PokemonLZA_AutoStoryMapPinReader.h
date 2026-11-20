/*  PokemonLZA AutoStory - Map pin reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the player stands on the full map (PLUS opens it, B closes it). The map
 *  does not scroll, so the arrow's pixel is the player's position.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMapPinReader_H
#define PokemonAutomation_PokemonLZA_AutoStoryMapPinReader_H

#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The centre of the largest blob of the arrow's saturated blue (at least a third of
//  the arrow) in Wild Zone 1's window; the cone, lake and water are paler. None on an
//  empty or tiny frame, or with no arrow.
std::optional<MapPoint> read_map_pin(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
