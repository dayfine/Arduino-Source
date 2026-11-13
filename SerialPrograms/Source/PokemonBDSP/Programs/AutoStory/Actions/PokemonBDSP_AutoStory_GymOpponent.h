/*  BDSP AutoStory - Gym opponent
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Who the person in front of us is on the way up the Oreburgh Gym. 01.04
 *  walks straight up the middle column and is stopped by the gym trainers;
 *  when a blocked walk and an A press opened a talk, the talk was taken for
 *  Roark's, and the second trainer's fight ended the step with no badge
 *  (lab-B, 2026-11-12 22:50). A count of trainers beaten fixed that, but the
 *  count is run state: a reload, or a save made mid-gym, restarts it at zero
 *  and Roark was called "Gym trainer 1 of 2" (lab-B, 23:49).
 *
 *  So the opponent is told by where we stand: every try walks the same column
 *  from the entrance, so the north distance walked since the entrance (the
 *  walks' held time over the measured tile pace) says which of the people
 *  stationed up the column stopped us. It does not depend on what was beaten
 *  before.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_GymOpponent_H
#define PokemonAutomation_PokemonBDSP_AutoStory_GymOpponent_H

#include <chrono>
#include <string>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Where we stand on the way up the gym's column, in tiles north of the
//  entrance, estimated from the time walked north.
class GymColumn{
public:
    //  `last_trainer_tiles`: how far north of the entrance the farthest gym
    //  trainer stops us. `leader_tiles`: the same for the tile in front of
    //  the leader. `ms_per_tile`: the pace walking north.
    GymColumn(double last_trainer_tiles, double leader_tiles, std::chrono::milliseconds ms_per_tile)
        : m_last_trainer(last_trainer_tiles)
        , m_leader(leader_tiles)
        , m_ms_per_tile(ms_per_tile.count() > 0 ? ms_per_tile.count() : 1)
    {}

    //  A try starts again at the entrance (the first, or after a whiteout).
    void restart(){
        m_walked_ms = 0;
    }
    //  A walk north held for `walked`.
    void walked_north(std::chrono::milliseconds walked){
        if (walked.count() > 0){
            m_walked_ms += walked.count();
        }
    }
    //  Tiles north of the entrance, estimated.
    double tiles() const{
        return (double)m_walked_ms / (double)m_ms_per_tile;
    }
    //  Whether the person in front of us is the leader: past the halfway
    //  point between the last trainer's stop and the leader's.
    bool leader_here() const{
        return tiles() >= (m_last_trainer + m_leader) / 2;
    }
    //  The opponent's name, for the log and the battle.
    std::string label() const{
        return leader_here() ? "Roark" : "Gym trainer";
    }

private:
    double m_last_trainer;
    double m_leader;
    long long m_ms_per_tile;
    long long m_walked_ms = 0;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
