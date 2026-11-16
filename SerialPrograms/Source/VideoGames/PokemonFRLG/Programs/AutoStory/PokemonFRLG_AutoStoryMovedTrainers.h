/*  AutoStory Moved Trainers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Trainers who walked up to the player, and stand there from then on.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMovedTrainers_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMovedTrainers_H

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRouteSteps.h"


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

//  The TRAINER_AT_FLAG that says `move`.
std::string trainer_at_flag(const TrainerMove& move);

//  The trainer who walks up to the player seen on `tile` of `maps` (a step
//  into their line, step_until_seen()), and the tile beside the player they
//  stop on, toward where they stood. Trainers `beaten`
//  names (trainer_id()) see no one. Nothing if no trainer may see the
//  player there, or if where they stop isn't known to one tile: two
//  trainers may see it, or one who wanders may come from either side.
//  Takashi, facing south on Viridian Gym's (10, 2), saw the player on
//  (10, 5) and walked to (10, 4), where 17.03's way north went (console B,
//  2026-11-15 16:52).
std::optional<TrainerMove> trainer_walking_up(
    const std::vector<MapData>& maps, const MapPosition& tile, const std::set<std::string>& beaten
);

//  `maps` with each trainer of `moved` standing on its new tile, no longer
//  a trainer: they fought the player when they walked up, and they stop
//  no one. A trainer not found is left as is.
std::vector<MapData> with_trainers_moved(std::vector<MapData> maps, const std::vector<TrainerMove>& moved);

//  After a step from `map_before` that ended on `map_after`: if the player
//  left the map, every trainer who walked up stands on their own tile
//  again, beaten: their TRAINER_AT_FLAG in `run_flags` becomes a
//  TRAINER_BEATEN_FLAG (PokemonFRLG_AutoStoryPacedRoute.h). The game
//  places a map's people from its object events each time it loads the
//  map. Jeff walked up to Route 6's (11, 33) in 05.10, and the run went on
//  to Vermilion City; back in 06.03 he stands on (13, 33) again, and the
//  leg north onto (11, 33) is open.
void trainers_back_after_map_change(std::string_view map_before, std::string_view map_after, std::set<std::string>& run_flags);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
