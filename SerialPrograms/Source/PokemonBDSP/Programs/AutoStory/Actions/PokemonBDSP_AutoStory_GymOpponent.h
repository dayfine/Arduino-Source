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
 *  Position is not identity either: it is dead reckoning from the time held,
 *  so it is only a hint (a log line, and whether to expect Roark's long
 *  pause). Nor is the intro text: read by OCR, it held only in the
 *  languages it had word tables for, and a French game fought both gym
 *  battles and Roark's blind, then failed (lab-B, 2026-11-13 01:17). Who the
 *  opponent was is told by what happens after the battle, in any language:
 *  Roark talks on the overworld (the Coal Badge, then the TM, about 40 s of
 *  lines at 01:16), while a gym trainer's lines are all in the battle and
 *  the overworld comes back quiet (lab-B 2026-11-12 22:49: the trainer's
 *  cleanup was over in 7.8 s, the fade and the 5 s quiet).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_GymOpponent_H
#define PokemonAutomation_PokemonBDSP_AutoStory_GymOpponent_H

#include <chrono>
#include <cstddef>
#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Who a won gym battle was against.
enum class GymOpponent{
    TRAINER,
    LEADER,
};
inline std::string_view gym_opponent_name(GymOpponent opponent){
    return opponent == GymOpponent::LEADER ? "Roark" : "a gym trainer";
}

//  This many looks with a box on the overworld after a won battle are the
//  leader's speech. Roark's lines fill dozens of looks; one look is allowed
//  to be a stray read of the battle's last frame.
constexpr size_t GYM_LEADER_TALK_LOOKS = 2;

//  Who a won gym battle was against, from the looks with a box on the
//  overworld after it (confirm_battle_overworld()).
inline GymOpponent gym_opponent_after_battle(size_t box_looks_after){
    return box_looks_after >= GYM_LEADER_TALK_LOOKS ? GymOpponent::LEADER : GymOpponent::TRAINER;
}

//  Where we stand on the way up the gym's column, in tiles north of the
//  entrance, estimated from the time walked north: a hint, not who is there.
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
