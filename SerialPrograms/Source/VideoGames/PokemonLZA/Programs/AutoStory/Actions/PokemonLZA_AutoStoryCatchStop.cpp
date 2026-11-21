/*  PokemonLZA AutoStory - Catch stop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "PokemonLZA_AutoStoryCatchStop.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


SpotVisit spot_visit(int spot, std::span<const int> caught_at){
    const bool caught = std::find(caught_at.begin(), caught_at.end(), spot) != caught_at.end();
    return caught ? SpotVisit::TURN_BACK : SpotVisit::CATCH;
}

AfterThrow after_throw(const ThrowSeen& seen, bool battle, const std::optional<LockTarget>& relocked){
    if (seen.registered){
        return AfterThrow::STOP_ENDS;
    }
    if (battle){
        return AfterThrow::WEAKEN;
    }
    if (seen.new_species()){
        return AfterThrow::STOP_ENDS;
    }
    if (relocked && relocked->caught != true){
        return AfterThrow::THROW_AGAIN;
    }
    return AfterThrow::LOOK_AGAIN;
}

bool CameraPitch::level_now(){
    const bool tipped = m_tipped;
    m_tipped = false;
    return tipped;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
