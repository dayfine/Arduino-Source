/*  Story Event
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Pokemon_StoryEvent.h"

namespace PokemonAutomation{
namespace Pokemon{


Condition condition(std::string name, Check check){
    return Condition{std::move(name), std::move(check)};
}

Condition pose_known(std::string map, PoseCertainty need){
    std::string name = std::format("pose {}{}", certainty_name(need), map.empty() ? "" : std::format(" on {}", map));
    return condition(std::move(name), [map = std::move(map), need](RunState& state){
        return (map.empty() || state.pose.map == map) && certainty_covers(state.pose.certainty, need);
    });
}

std::string recovery_name(RecoveryKind kind){
    switch (kind){
    case RecoveryKind::WAIT:
        return "wait";
    case RecoveryKind::REANCHOR:
        return "re-anchor";
    case RecoveryKind::STOP:
        return "stop";
    }
    return "?";
}



} // namespace Pokemon
} // namespace PokemonAutomation
