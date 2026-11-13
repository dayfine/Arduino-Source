/*  BDSP AutoStory - Gym opponent
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Who the person in front of us is on the way up the Oreburgh Gym. 01.04
 *  walks straight up and is stopped by the gym trainers; when the walk was
 *  blocked and an A press opened a talk, the talk was taken for Roark's, and
 *  the second trainer's fight ended the step with no badge (lab-B,
 *  2026-11-12 22:50). Roark is next only once every trainer in the gym is
 *  beaten; a talk before that is a trainer's.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_GymOpponent_H
#define PokemonAutomation_PokemonBDSP_AutoStory_GymOpponent_H

#include <cstddef>
#include <string>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The gym trainers beaten so far, kept across the tries of a step: a trainer
//  beaten stays beaten after a whiteout.
class GymTrainers{
public:
    //  `total`: the trainers between the door and the leader.
    explicit GymTrainers(size_t total)
        : m_total(total)
    {}

    //  A trainer's battle was won.
    void record_win(){
        if (m_beaten < m_total){
            m_beaten++;
        }
    }
    //  Whether the next opponent is the leader.
    bool leader_next() const{
        return m_beaten >= m_total;
    }
    //  The next opponent's name, for the log and the battle.
    std::string next_label() const{
        if (leader_next()){
            return "Roark";
        }
        return "Gym trainer " + std::to_string(m_beaten + 1) + " of " + std::to_string(m_total);
    }

private:
    size_t m_total;
    size_t m_beaten = 0;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
