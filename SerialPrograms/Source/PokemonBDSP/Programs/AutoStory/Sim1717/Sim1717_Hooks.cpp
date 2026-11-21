/*  BDSP AutoStory PR #1717 Simulation Hooks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Sim1717_Hooks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{
thread_local Sim1717Hooks* g_hooks = nullptr;
}

Sim1717Hooks* get_sim1717_hooks(){
    return g_hooks;
}

void set_sim1717_hooks(Sim1717Hooks* hooks){
    g_hooks = hooks;
}

std::chrono::steady_clock::time_point sim1717_steady_now(){
    if (Sim1717Hooks* hooks = g_hooks){
        return hooks->steady_now();
    }
    return std::chrono::steady_clock::now();
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
