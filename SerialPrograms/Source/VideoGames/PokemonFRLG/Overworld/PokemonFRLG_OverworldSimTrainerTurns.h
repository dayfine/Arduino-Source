/*  Overworld Sim Trainer Turns
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The sim's trainers who turn on a timer, and the way one faces at a time.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldSimTrainerTurns_H
#define PokemonAutomation_PokemonFRLG_OverworldSimTrainerTurns_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonFRLG_OverworldRouteSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A trainer who turns on a timer (pokefirered MovementType_FaceDownAndRight
//  and its kin): `ways[0]` at first, then after 32, 48, 64 or 80 frames
//  (gMovementDelaysShort) one of `ways` at random, from `seed`, and again.
//  They turn to the player only when the player runs
//  (TryGetTrainerEncounterDirection()), which the sim's player doesn't, and
//  see only the way they face.
struct SimTrainerTurns{
    std::vector<Direction> ways;
    uint32_t seed = 0;
};

//  The way the trainer standing on `home` faces at `when` on the sim's
//  clock, if `turns` (by to_string(MapPosition)) has them.
std::optional<Direction> trainer_turns_facing(
    const std::map<std::string, SimTrainerTurns>& turns, const MapPosition& home, Milliseconds when
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
