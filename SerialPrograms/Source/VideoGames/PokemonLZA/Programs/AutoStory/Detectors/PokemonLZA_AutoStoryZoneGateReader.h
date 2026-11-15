/*  PokemonLZA AutoStory - Zone gate reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A wild zone's entrance gate in the overworld, by colour and shape.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZoneGateReader_H
#define PokemonAutomation_PokemonLZA_AutoStoryZoneGateReader_H

#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A wild zone's entrance gate: two tall pillars of the zone's saturated
//  lime green, a statue on each, with the zone's emblem hanging between
//  them. From mid-street ~20 m before Wild Zone 1's gate each pillar is
//  ~3% of the width wide and ~18% of the height tall, ~3 widths apart
//.
struct ZoneGateSighting{
    //  Halfway between the pillars' outer edges, 0 (left edge) to 1 (right
    //  edge).
    double x = 0;
    //  From the left pillar's left edge to the right pillar's right edge,
    //  as a share of the screen's width: it grows as the player nears.
    double width = 0;
};

//  The gate on `screen`, if it shows: the largest pair of pillar-shaped
//  lime blobs (solid, at least twice as tall as wide) of about the same
//  size, standing on about the same line, 1.5 to 8 widths apart with
//  little lime between them. The zone's wall, the same green, is many
//  thin stripes and a grid: its blobs are not solid, and those that come
//  near touch or overlap (a walk's miss beside the wall).
//  Language-independent: no text is read.
std::optional<ZoneGateSighting> read_zone_gate(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
