/*  BDSP AutoStory - Pokemon Center
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Center_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Center_H

#include <optional>
#include <string>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteTiming.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  After a wild battle out of doors: fails with a screenshot if the screen
//  turns into a room within 10 s, which means the party fainted and the
//  game took us home (r258 lost its third training battle in Route 202's
//  grass, woke in Twinleaf, and walked the Sandgem plan there). Returns
//  once the outside shows, not black and not a room.
void fail_if_blacked_out(VideoStream& stream, ProControllerContext& context, const std::string& label);

//  For a player on the row below a building's door, its lit doorway on
//  screen: steps across by the doorway's offset, looking again after each
//  step, until it is straight ahead. Does not go in, and does not turn north
//  to face it: a turn there walked us in (r127c). Returns false if no
//  doorway shows, a Center's inside shows, or after 6 steps.
bool line_up_with_lit_door(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
);

//  Goes in through a building's door (a Pokemon Center's or a Poke Mart's).
//  Starts with its lit doorway on screen, within a few tiles: lines up with
//  the doorway by sight and walks in, looking again after every move. Whole-
//  tile steps sideways miss it, since movement is not locked to tiles, and
//  its fade may not read as black: a new scene also counts as inside. Fails
//  with a screenshot when no doorway shows, or after 12 moves.
void enter_lit_door(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
);

//  Heals the party at a Pokemon Center. Starts in front of the Center with its
//  lit doorway on screen, within a few tiles of it: lines up with the doorway
//  by sight, walks in, talks to the nurse, accepts, and walks back out. Ends
//  on the tile below the door.
void heal_at_pokemon_center(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
);

//  heal_at_pokemon_center() from just inside the door: talks to the nurse,
//  accepts, and walks back out. Ends on the tile below the door. For a caller
//  that walked in on its own (from a known tile, straight north).
void heal_inside_pokemon_center(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
);

//  At most this many tiles from a Center's door mat up to the nurse's
//  counter, straight north (a bound, not measured: the counter is the wall
//  that ends the run, so going on is harmless).
constexpr size_t CENTER_MAT_TO_COUNTER_TILES = 8;
//  The one held run (B and up) from the door mat to the counter: long
//  enough at walking pace too, before the running shoes.
constexpr Milliseconds NURSE_RUN_HOLD =
    ROUTE_START_DELAY + ROUTE_MS_PER_TILE_UP_DOWN * (int64_t)CENTER_MAT_TO_COUNTER_TILES;

//  From just inside a Pokemon Center's door: walks up to the nurse, talks to

//  her and accepts. Ends at her counter, for a caller that walks out its own
//  way.
void talk_to_nurse(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
