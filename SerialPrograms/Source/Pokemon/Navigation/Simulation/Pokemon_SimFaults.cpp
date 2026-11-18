/*  Sim Faults
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "Pokemon_SimFaults.h"

namespace PokemonAutomation{
namespace Pokemon{


const std::vector<FaultClass>& fault_classes(){
    static const std::vector<FaultClass> classes{
        FaultClass::SCENE_END_POSE,
        FaultClass::SCENE_EARLY,
        FaultClass::BATTLE_OPENING,
        FaultClass::WILD_BATTLES,
        FaultClass::LATE_FADE,
        FaultClass::SLOW_FADE_IN,
        FaultClass::WARP_NO_OP,
        FaultClass::DARK_CAVE,
        FaultClass::LOST_TAP,
        FaultClass::UNSEEN_STEP,
        FaultClass::MOUNT_LOST,
    };
    return classes;
}

std::string_view fault_class_name(FaultClass fault){
    switch (fault){
    case FaultClass::SCENE_END_POSE:
        return "scene end pose";
    case FaultClass::SCENE_EARLY:
        return "scene early";
    case FaultClass::BATTLE_OPENING:
        return "battle opening";
    case FaultClass::WILD_BATTLES:
        return "wild battles";
    case FaultClass::LATE_FADE:
        return "late fade";
    case FaultClass::SLOW_FADE_IN:
        return "slow fade-in";
    case FaultClass::WARP_NO_OP:
        return "warp no-op";
    case FaultClass::DARK_CAVE:
        return "dark cave";
    case FaultClass::LOST_TAP:
        return "lost tap";
    case FaultClass::UNSEEN_STEP:
        return "unseen step";
    case FaultClass::MOUNT_LOST:
        return "mount lost";
    }
    return "?";
}


FaultOccurrences::FaultOccurrences(std::set<int> faulted)
    : m_numbers(std::move(faulted))
{}

bool FaultOccurrences::next(){
    m_chances++;
    if (!m_numbers.contains(m_chances)){
        return false;
    }
    m_faulted++;
    return true;
}


SimFaults::SimFaults(std::map<FaultClass, std::set<int>> faulted){
    for (auto& [fault, numbers] : faulted){
        m_classes.emplace(fault, FaultOccurrences(std::move(numbers)));
    }
}

bool SimFaults::next(FaultClass fault){
    return m_classes[fault].next();
}

int SimFaults::chances(FaultClass fault) const{
    const auto iter = m_classes.find(fault);
    return iter == m_classes.end() ? 0 : iter->second.chances();
}

int SimFaults::faulted(FaultClass fault) const{
    const auto iter = m_classes.find(fault);
    return iter == m_classes.end() ? 0 : iter->second.faulted();
}



} // namespace Pokemon
} // namespace PokemonAutomation
