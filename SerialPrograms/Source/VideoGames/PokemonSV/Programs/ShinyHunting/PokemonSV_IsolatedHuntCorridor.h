/*  Isolated Uphill Corridor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#ifndef PokemonAutomation_PokemonSV_IsolatedHuntCorridor_H
#define PokemonAutomation_PokemonSV_IsolatedHuntCorridor_H
#include "PokemonSV_IsolatedHuntLoop.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
uint64_t isolated_stage_timeout_ms(uint64_t started, uint64_t deadline, uint64_t requested);
bool valid_isolated_ascent(const IsolatedAscentLimits& limits);
IsolatedHuntResult run_isolated_corridor(const IsolatedHuntRequest& request,
    const IsolatedHuntLimits& limits, IsolatedHuntActions& actions,
    uint64_t power_started, uint64_t hunt_started);
}
}
}
#endif
