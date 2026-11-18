/*  Gate Tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a gate state (Pokemon_GateState.h) makes of a map's tiles: a closed
 *  gate is a wall; an open one is the tile its `open_glyph` names.
 *
 */

#ifndef PokemonAutomation_Pokemon_GateTiles_H
#define PokemonAutomation_Pokemon_GateTiles_H

#include <string>
#include "Pokemon_GateState.h"
#include "Pokemon_Grid.h"
#include "Pokemon_MapData.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The name GateState::switches_on knows `pressed` by: its id when several
//  maps share it (Pokemon Mansion's floors), and gate_key(its map, its id)
//  when it is one map's own (each Victory Road floor has a
//  "floor_switch_1" of its own, with its own flag).
std::string switch_key(const MapSwitch& pressed);

//  The switch whose boulder button is `pos`, or nullptr if no switch's is.
const MapSwitch* switch_with_button(const MapData& map, const TilePos& pos);

//  The gate that has `pos`, or nullptr if no gate does.
const MapGate* gate_at(const MapData& map, const TilePos& pos);

//  Whether `gate` of `map` is open: a card-key gate is open at a new game,
//  if the map says so, or once opened; a switch's gate is open while the
//  switch is in its `open_when` state.
bool gate_is_open(const MapData& map, const MapGate& gate, const GateState& state);

//  The tile at `pos` for walking: a gate tile is a wall while its gate is
//  closed and its `open_glyph` tile once open; any other tile is as it is.
//  `pos` must be inside the map.
TileKind resolved_tile(const MapData& map, const TilePos& pos, const GateState& state);

//  All the map's tiles, resolved.
Grid<TileKind> resolve_tiles(const MapData& map, const GateState& state);

//  The closed card-key gate that a player standing on `from` and facing
//  `direction` faces, or nullptr if there is none (a wall, an open gate, a
//  switch's gate).
const MapGate* closed_card_key_gate_faced(const MapData& map, const TilePos& from, Direction direction, const GateState& state);

//  The switch with a statue that a player standing on `from` and facing
//  `direction` presses with A, or nullptr if there is none.
const MapSwitch* switch_pressed(const MapData& map, const TilePos& from, Direction direction);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
