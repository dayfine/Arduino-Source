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
#include <string_view>
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The closest built-in reference within the match distance of `screen`'s minimap
//  ring (gain-matched; the player's arrow and the rim's icons left out).
std::optional<std::string> matching_minimap(const ImageViewRGB32& screen);

//  The minimap on `screen` as a ReferenceData/plza-minimap-<name>.json object.
JsonObject minimap_reference_json(const ImageViewRGB32& screen, std::string_view source);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
