/*  AutoStory Moved Trainers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Trainers who walked up to the player, and stand there from then on.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMovedTrainers_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMovedTrainers_H

#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/Navigation/Pokemon_MapData.h"


namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The maps a story is planned on (Pokemon_MapData.h).
using Pokemon::MapData;


//  A trainer who saw the player walked up to the tile beside them and
//  fought: the game leaves them standing there, not on the map's tile. The
//  trainer (trainer_id(), from the map's tile) and where they stand now.
struct TrainerMove{
    std::string trainer;
    int x = 0;
    int y = 0;
};

//  A run flag that says where a trainer stands since they walked up to the
//  player: this prefix, then the trainer's trainer_id(), "=", the tile
//  ("trainer_at:SilphCo_5F/rocket_m@9,21=11,21"). The step that fought
//  them sets it in flags_after and the checkpoints after start with it:
//  the Rocket of 16.02 walked from 5F's (9, 21) up to (11, 21) and stood
//  across 16.03's leg along row 21, which the planner had through his
//  map tile's neighbour (console B, 2026-11-14, run 4).
inline constexpr std::string_view TRAINER_AT_FLAG = "trainer_at:";

//  The trainers `run_flags` says walked up to the player (TRAINER_AT_FLAG).
//  A flag that doesn't parse throws ParseException.
std::vector<TrainerMove> moved_trainers(const std::set<std::string>& run_flags);

//  `maps` with each trainer of `moved` standing on its new tile, no longer
//  a trainer: they fought the player when they walked up, and they stop
//  no one. A trainer not found is left as is.
std::vector<MapData> with_trainers_moved(std::vector<MapData> maps, const std::vector<TrainerMove>& moved);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
