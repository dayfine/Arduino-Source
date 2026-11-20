/*  PokemonLZA AutoStory - Minimap match
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the player stands, by the minimap at the top left against stored
 *  references: ReferenceData/plza-minimap-<name>.json, each the minimap at
 *  one place averaged into cells. The minimap is north up, so the same
 *  place shows the same map whichever way the player and camera face.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMinimapMatch_H
#define PokemonAutomation_PokemonLZA_AutoStoryMinimapMatch_H

#include <optional>
#include <string>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The nearest built-in reference to `screen`'s minimap ring within the match
//  distance (gain-matched; the player's arrow and the rim's icons left out).
std::optional<std::string> matching_minimap(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
