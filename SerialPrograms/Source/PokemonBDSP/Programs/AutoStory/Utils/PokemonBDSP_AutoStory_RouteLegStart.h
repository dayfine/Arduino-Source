/*  BDSP AutoStory - Route leg start
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the planned route walker does as each leg starts: the leg's line in
 *  the log, then the run's breakpoint before it.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteLegStart_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteLegStart_H

#include <cstddef>
#include "Pokemon/AutoStory/Types/Pokemon_StoryBreakpoints.h"
#include "Pokemon/Navigation/Pokemon_RouteLogLegs.h"
#include "PokemonBDSP_AutoStory_RouteIO.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A leg is about to be walked: its line to `io`'s log (note(), STATUS),
//  then the breakpoint before it, if the run has `pause`.
void leg_started(RouteIO& io, size_t number, size_t count, const Pokemon::PlannedRouteLeg& leg, Pokemon::StoryPausePoints* pause);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
