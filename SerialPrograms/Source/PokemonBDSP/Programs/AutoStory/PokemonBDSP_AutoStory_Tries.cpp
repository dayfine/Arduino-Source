/*  BDSP AutoStory - Bounded tries
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Tries.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


std::optional<size_t> try_bounded(
    size_t tries,
    const std::function<bool(size_t attempt)>& attempt,
    const std::function<void(size_t attempt)>& between
){
    for (size_t current = 1; current <= tries; current++){
        if (attempt(current)){
            return current;
        }
        if (current < tries){
            between(current);
        }
    }
    return std::nullopt;
}



}
}
}
