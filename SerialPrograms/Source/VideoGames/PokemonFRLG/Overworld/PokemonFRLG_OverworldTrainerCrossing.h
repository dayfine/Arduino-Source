/*  Overworld Trainer Crossing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a leg crosses the sight of its maps' trainers, and how far one
 *  may walk up to the player; and how long the walker watches for one
 *  after such a leg.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldTrainerCrossing_H
#define PokemonAutomation_PokemonFRLG_OverworldTrainerCrossing_H

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Where a WALK leg crosses the sight of its maps' trainers
//  (trainer_may_see()): the tiles of the leg after its start that one may
//  see, in order, and the farthest any of them walks up to the player
//  (their sight), to watch for them that long. The trainers `beaten`
//  names (trainer_id()) are left out: they stop no one.
struct TrainerCrossing{
    std::vector<MapPosition> seen;
    int approach_tiles = 0;
    //  The leg's tiles after the last one a trainer may see: a trainer who
    //  saw the player there froze them while the hold went on that long.
    int tiles_after_seen = 0;
};
//  Nothing if `step`, from `start`, crosses no unbeaten trainer's sight.
std::optional<TrainerCrossing> trainer_crossing(
    const std::vector<MapData>& maps, const MapPosition& start, const RouteStep& step,
    const std::set<std::string>& beaten = {}
);
//  The trainers of `beaten` (trainer_id()) whose sight `step`, from
//  `start`, crosses, each once: the walker logs them and doesn't watch.
std::vector<std::string> beaten_trainers_crossed(
    const std::vector<MapData>& maps, const MapPosition& start, const RouteStep& step,
    const std::set<std::string>& beaten
);


//  The full watch for a trainer after a leg that crosses their sight: the
//  "!", their walk up and their message, from the hold's end. On Route 3
//  (console B, 2026-11-02) Lass Robin's message showed after the 500 ms
//  watch after the leg had ended. Not measured on the console yet.
inline constexpr Milliseconds TRAINER_NOTICE(1500);
//  From the walk up's end to the message.
inline constexpr Milliseconds TRAINER_SPEAKS(1000);
//  The full watch for a trainer `approach_tiles` away.
Milliseconds trainer_full_watch(int approach_tiles);
//  The shortest first look after a leg across a trainer's sight. No "!"
//  detector exists yet: the look sees a box (the trainer's message or
//  battle), which shows only after the "!" and the walk up. A 600 ms look
//  alone missed the picnicker who saw the last tile of Cerulean Gym's
//  "west 1" (the sim). Not measured on the console yet.
inline constexpr Milliseconds TRAINER_FIRST_LOOK(600);
//  How long the walker first looks after a leg with `crossing`: the full
//  watch, less the time the hold went on after the last tile a trainer may
//  see (at the fastest pace, RUN_TILE_TIME a tile), but no less than
//  TRAINER_FIRST_LOOK. Anything seen extends it to the full watch.
Milliseconds trainer_first_look(const TrainerCrossing& crossing);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
