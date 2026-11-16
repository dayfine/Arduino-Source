/*  AutoStory Moved Boulders
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the Strength boulders a run pushed stand, kept as run flags, and
 *  the maps the walks after plan on, with the boulders there.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMovedBoulders_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMovedBoulders_H

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_AutoStoryCheckpoints.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A Strength boulder pushed off its map tile: the map, the tile the map
//  has it on, and where it stands now.
struct BoulderMove{
    std::string map;
    TilePos from;
    TilePos at;
};

//  The boulders `run_flags` says were pushed: a run flag each,
//  "boulder_at:<map>@<x>,<y>=<x>,<y>", the boulder's map tile, then the
//  tile it stands on now ("boulder_at:VictoryRoad_1F@7,18=7,19"). The
//  walks after a push plan around it there (run_story_maps()); with the
//  boulder on its map tile instead, by_boulder_2_corner of 19.04b planned
//  south from (7, 18) onto the boulder pushed to (7, 19), bumped it, and
//  every leg after ran a row off (console B, 2026-11-16). A flag that
//  doesn't parse throws ParseException.
std::vector<BoulderMove> moved_boulders(const std::set<std::string>& run_flags);

//  Records in `run_flags` a push of the boulder `direction` of the player's
//  `start` by `tiles` tiles: a boulder already pushed keeps its map tile in
//  the flag.
void note_boulder_pushed(const MapPosition& start, Direction direction, int64_t tiles, std::set<std::string>& run_flags);

//  `maps` with each boulder of `moved` on the tile it was pushed to, or
//  gone if that tile is a hole it fell through: with it standing on 3F's
//  hole (34, 18), 19.04d's drop_to_2f went round by 3F's west room into
//  Cooltrainer Colby's line (console B, 2026-11-16).
std::vector<MapData> with_boulders_moved(std::vector<MapData> maps, const std::vector<BoulderMove>& moved);

//  After a step from `map_before` that ended on `map_after`: if the player
//  left the map, the game put its boulders back on their map tiles, and
//  their flags leave `run_flags`. Not a boulder on a floor switch's button:
//  Victory Road's switch scripts keep it there for good
//  (copyobjectxytoperm in pokefirered's VictoryRoad_*_EventScript_FloorSwitch).
void boulders_back_after_map_change(std::string_view map_before, std::string_view map_after, std::set<std::string>& run_flags);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
