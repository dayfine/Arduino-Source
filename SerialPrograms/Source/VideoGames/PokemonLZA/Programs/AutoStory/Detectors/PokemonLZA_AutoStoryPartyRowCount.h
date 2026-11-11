/*  PokemonLZA AutoStory - Party row count
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How many Pokemon the party bar's small slots show, read from the HP bars
 *  under them.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryPartyRowCount_H
#define PokemonAutomation_PokemonLZA_AutoStoryPartyRowCount_H

#include <cstddef>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  How many of the five small slots under the lead's card (party members 2
//  to 6) hold a Pokemon. A filled slot has an HP bar under it at y 1028-1032
//  on 1920x1080, x 160-210 for the first and 76.6 more for each next one:
//  green (about 59, 192, 113 on console B, 2026-11-10), or yellow or red when
//  hurt; an empty slot shows the scene there. Only in the overworld, where
//  the party bar shows.
size_t party_row_count(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
