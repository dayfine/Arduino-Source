/*  Overworld Sim Trainers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The sim's trainers: who sees the player, and who turns on a timer.
 *
 */

#include <cstdlib>
#include <format>
#include <random>
#include "PokemonFRLG_OverworldObjects.h"
#include "PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_OverworldSimTrainerTurns.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::optional<Direction> trainer_turns_facing(
    const std::map<std::string, SimTrainerTurns>& turns, const MapPosition& home, Milliseconds when
){
    const auto found = turns.find(to_string(home));
    if (found == turns.end() || found->second.ways.empty()){
        return std::nullopt;
    }
    const SimTrainerTurns& trainer = found->second;
    static constexpr int64_t DELAY_FRAMES[] = {32, 48, 64, 80};
    std::mt19937 rng(trainer.seed);
    Direction facing = trainer.ways[0];
    //  Each turn: the delay, then a frame for the turn itself; 60 frames a
    //  second.
    int64_t frame = 0;
    while (true){
        frame += DELAY_FRAMES[rng() & 3] + 1;
        if (frame * 1000 / 60 > when.count()){
            return facing;
        }
        facing = trainer.ways[rng() % trainer.ways.size()];
    }
}

std::optional<Direction> trainer_facing(const OverworldSim& sim, const MapPosition& home){
    return trainer_turns_facing(sim.m_script.trainer_turns, home, sim.m_now);
}

namespace{

//  The tile next to `player` a trainer standing on `box` walks up to.
TilePos trainer_stop(const Pokemon::TileBox& box, const TilePos& player){
    if (box.x0 <= player.x && player.x <= box.x1){
        return TilePos{player.x, box.y0 > player.y ? player.y + 1 : player.y - 1};
    }
    return TilePos{box.x0 > player.x ? player.x + 1 : player.x - 1, player.y};
}

} // namespace

//  A trainer on the player's map who sees them now walks up and speaks.
bool OverworldSim::trainer_sees(Milliseconds pressed){
    std::vector<MapObject>& objects = m_maps[m_map].objects;
    for (size_t c = 0; c < objects.size(); c++){
        MapObject& trainer = objects[c];
        if (!trainer.trainer){
            continue;
        }
        //  One who turns on a timer sees only the way they face as the step
        //  ends.
        MapObject looking = trainer;
        if (const std::optional<Direction> facing = trainer_turns_facing(m_script.trainer_turns, MapPosition{map().name, {trainer.x, trainer.y}}, pressed + tile_time())){
            looking.trainer->looks = {*facing};
        }
        if (!trainer_may_see(map(), looking, m_player.position)){
            continue;
        }
        const Pokemon::TileBox box = trainer.wander.value_or(Pokemon::TileBox{trainer.x, trainer.y, trainer.x, trainer.y});
        const TilePos stop = trainer_stop(box, m_player.position);
        const int approach = std::abs(stop.x - trainer.x) + std::abs(stop.y - trainer.y);
        trainer.x = stop.x;
        trainer.y = stop.y;
        trainer.wander.reset();
        //  The player turns to face the trainer.
        for (Direction direction : {Direction::NORTH, Direction::EAST, Direction::SOUTH, Direction::WEST}){
            m_state.facing = m_player.position.next(direction) == stop ? direction : m_state.facing;
        }
        m_trainer_speaking = c;
        m_frozen = true;
        schedule(pressed + m_script.timing.tile * (1 + approach) + m_script.timing.trainer_notice, SimScreen::MESSAGE, false);
        note(std::format("{} on {} sees the player, walks up {} tile(s) to {}.", trainer.name, map().name, approach, to_string(stop)));
        return true;
    }
    return false;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
