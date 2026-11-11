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
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  While ZL locks on to a wild Pokemon: true when its name plate shows the
//  caught mark, false when the plate shows none, nothing when no plate was
//  found. The plate is found by its HP bar, a green bar (about 55, 195, 120)
//  at least 60 px wide on 1920x1080, in the upper two thirds of the
//  screen; the mark, red over white, sits above the bar's right end
//  (0.6-1.5 bar widths from its left, 0.45-0.85 above it). Tuned on Mareep,
//  caught and not, on console B (2026-11-10).
std::optional<bool> lock_target_caught(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
