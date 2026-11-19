/*  PokemonLZA AutoStory - Caught mark reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Whether the wild Pokemon locked on to is of a species already caught:
 *  its name plate then shows a Poke Ball after the level and gender.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCaughtMarkReader_H
#define PokemonAutomation_PokemonLZA_AutoStoryCaughtMarkReader_H

#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  While ZL locks on: whether the name plate shows the caught mark; nothing when no
//  plate was found. The plate is found by its HP bar; the mark, pinkish red over white,
//  sits above the bar's right end, right of the orange gender mark. The plate shows
//  whether or not the partner is out.
std::optional<bool> lock_target_caught(const ImageViewRGB32& screen);

//  While ZL locks on: the plate's HP bar as screen shares, nothing when no plate was
//  found. A lower bar is a nearer target.
std::optional<ImageFloatBox> lock_target_plate_bar(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
