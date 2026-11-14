/*  Overworld Objects
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The objects standing on a map: which one is on a tile, what a
 *  trainer may see, and the ones the game hides by flag.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldObjects_H
#define PokemonAutomation_PokemonFRLG_OverworldObjects_H

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "PokemonFRLG_OverworldTileRules.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The object standing, or maybe wandering, on this tile.
const MapObject* object_at(const MapData& map, const TilePos& point);

//  Whether `object`, a trainer, may see the player on `tile`: up to its
//  sight ahead, any way it looks, short of a wall, from its tile or, for
//  one who wanders, from any tile it may wander to. False for an object
//  that isn't a trainer.
bool trainer_may_see(const MapData& map, const MapObject& object, const TilePos& tile);

//  Removes the objects the game hides once one of `flags` is set.
void hide_objects(MapData& map, const std::set<std::string>& flags);

//  What a run calls the trainer `object` of `map` once beaten:
//  "<map>/<object>@<x>,<y>", from where the map places them, since a map
//  has several of a kind (Route 21's swimmers).
std::string trainer_id(const MapData& map, const MapObject& object);

//  `maps` with the trainers `beaten` names (trainer_id()) no longer
//  trainers: they stand where they are and stop no one, as in the game.
std::vector<MapData> with_trainers_beaten(std::vector<MapData> maps, const std::set<std::string>& beaten);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
