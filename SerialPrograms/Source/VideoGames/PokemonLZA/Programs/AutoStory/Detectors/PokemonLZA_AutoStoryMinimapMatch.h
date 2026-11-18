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


//  The name of the built-in reference the minimap on `screen` matches, if
//  any: within the match distance of it (cells_rmsd() over the map's ring,
//  the player's arrow in the middle and the rim's icons left out, after
//  scaling each channel to the reference's mean, so a darker or lighter
//  screen still matches), the closest when several are.
std::optional<std::string> matching_minimap(const ImageViewRGB32& screen);

//  The minimap on `screen` as a reference file's JSON (ReferenceData/
//  plza-minimap-<name>.json): {"source": `source` and the cells' square,
//  "grid": its 16 x 16 cells}. matching_minimap() compares the grid's ring.
JsonObject minimap_reference_json(const ImageViewRGB32& screen, std::string_view source);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
