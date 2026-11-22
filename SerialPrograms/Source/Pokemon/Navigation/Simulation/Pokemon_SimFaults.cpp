/*  Sim Faults
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "Common/Cpp/EnumStringMap.h"
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_SimFaults.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

const EnumStringMap<FaultClass>& fault_class_names(){
    static const EnumStringMap<FaultClass> MAP{
        {FaultClass::SCENE_END_POSE, "scene end pose"},
        {FaultClass::SCENE_EARLY,    "scene early"},
        {FaultClass::BATTLE_OPENING, "battle opening"},
        {FaultClass::WILD_BATTLES,   "wild battles"},
        {FaultClass::LATE_FADE,      "late fade"},
        {FaultClass::SLOW_FADE_IN,   "slow fade-in"},
        {FaultClass::WARP_NO_OP,     "warp no-op"},
        {FaultClass::DARK_CAVE,      "dark cave"},
        {FaultClass::LOST_TAP,       "lost tap"},
        {FaultClass::UNSEEN_STEP,    "unseen step"},
        {FaultClass::MOUNT_LOST,     "mount lost"},
    };
    return MAP;
}

} // namespace


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
    try{
        return fault_class_names().get_string(fault);
    }catch (const ParseException&){
        return "?";
    }
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


SimWandererBlocks::SimWandererBlocks(std::vector<SimWandererBlock> blocks)
    : m_blocks(std::move(blocks))
{}

bool SimWandererBlocks::blocks(std::string_view map, const TilePos& tile, Milliseconds now){
    for (size_t i = 0; i < m_blocks.size(); i++){
        const SimWandererBlock& block = m_blocks[i];
        if (block.map != map || block.tile != tile){
            continue;
        }
        if (now < block.from_time || (block.until_time && now >= *block.until_time)){
            continue;
        }
        if (block.clear_after_waits && *block.clear_after_waits <= 0){
            continue;
        }
        m_hit.insert(i);
        return true;
    }
    return false;
}

void SimWandererBlocks::on_wait(){
    for (size_t index : m_hit){
        SimWandererBlock& block = m_blocks[index];
        if (block.clear_after_waits && *block.clear_after_waits > 0){
            (*block.clear_after_waits)--;
        }
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
