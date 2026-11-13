/*  Overworld Spinner Tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Spinner tiles: which way their arrows slide the player, and where a
 *  slide ends.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldSpin_H
#define PokemonAutomation_PokemonFRLG_OverworldSpin_H

#include <optional>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Spinner tiles (Rocket Hideout B2F and B3F, Viridian Gym): pokefirered's
//  MB_SPIN_UP, MB_SPIN_DOWN, MB_SPIN_LEFT, MB_SPIN_RIGHT and
//  MB_STOP_SPINNING. The hideout's floors have them coded
//  (coded_spin_direction(), PokemonFRLG_CodedRooms.h); Viridian Gym's map
//  file writes them as glyphs ('u', 'd', 'l', 'r' and the stop 'o'); another
//  map may list them in these regions, as it does WILD_REGION.
//  "spin_north": MB_SPIN_UP.
//  "spin_south": MB_SPIN_DOWN.
//  "spin_west": MB_SPIN_LEFT.
//  "spin_east": MB_SPIN_RIGHT.
//  "spin_stop": MB_STOP_SPINNING.
//
//  The way the arrow on this tile slides the player, if it is a spinner.
std::optional<Direction> spin_direction(const MapData& map, const TilePos& point);
//  Whether this tile ends a slide (MB_STOP_SPINNING).
bool stops_spinning(const MapData& map, const TilePos& point);

//  Where a slide leaves the player, the way it last moved them, and how
//  many tiles it took.
struct SpinSlide{
    PlayerState end;
    Direction facing = Direction::NORTH;
    int tiles = 0;
};
//  The slide of a player who has just stepped onto the spinner `state`
//  stands on (pokefirered TryUpdatePlayerSpinDirection()): a tile at a
//  time the last arrow's way, turned by each arrow on the way, until a stop
//  tile, or a step the step rules refuse (a wall, a person: the game's
//  collision ends the forced movement there). Nothing if `state` isn't on a
//  spinner, if the arrows turn the player round in a loop, or if the slide
//  crosses a warp, which isn't modelled.
std::optional<SpinSlide> spin_slide(const MapData& map, const PlayerState& state);

//  Register the tests of spinner tiles, the lift's panel and the poster
//  switch on made-up maps.
void add_tests_OverworldSpin(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
