/*  PokemonLZA AutoStory - Map pin reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the player stands on the full map (PLUS opens it, "MAPPA" in
 *  Italian; B closes it). The map does not scroll with the player: Wild
 *  Zone 1 is drawn in the same place every time (console B, 2026-11-16),
 *  so the arrow's pixel is the player's position.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMapPinReader_H
#define PokemonAutomation_PokemonLZA_AutoStoryMapPinReader_H

#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A point on the full map, in the frame's pixels scaled to 800 x 450.
//  Wild Zone 1's green outline spans about x 280 to 515, y 190 to 260;
//  the objective's "!" (the far gate) is at about (313, 255).
struct MapPoint{
    double x = 0;
    double y = 0;
};

//  The player's arrow on the full map, if it shows in Wild Zone 1's
//  window (x 240 to 540, y 170 to 290): the centre of the largest blob of
//  the arrow's saturated blue, about (0, 160, 255) on console B, of at
//  least a third of the arrow's size (~500 pixels at 1920 x 1080). The
//  camera's lighter cone, the lake and the water are paler blues and do
//  not count. None on an empty or tiny frame, or with no arrow.
std::optional<MapPoint> read_map_pin(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
