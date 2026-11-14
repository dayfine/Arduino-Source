/*  PokemonLZA AutoStory - Walks by the objective marker's distance
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks that keep the camera on the main mission's objective marker
 *  (read_objective_marker(), PokemonLZA_AutoStoryObjectiveMarkerReader.h)
 *  and stop at a distance it reads: 03.08's road stops back from it, and
 *  03.07d's last steps to 03.08's start in Wild Zone 1.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMarkerWalks_H
#define PokemonAutomation_PokemonLZA_AutoStoryMarkerWalks_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Off screen, the game pins the marker to the edge with no digits
//  (anything read there is noise): turns the camera to it. Returns whether
//  the marker at `x` (0 to 1) was pinned.
bool turn_to_pinned_marker(ProControllerContext& context, double x);

//  Walks back away from the objective marker (the stick down: toward the
//  camera, which stays on the marker) until its distance reads `stop_at`
//  or more, or `limit`. A read more than 12 from the last is a misread, as
//  in walk_to_objective(). Returns the last distance read.
std::optional<int> walk_from_objective(ConsoleHandle& console, ProControllerContext& context, int stop_at, Milliseconds limit);

//  03.07d's last step, from just inside Wild Zone 1's entrance gate to
//  where 03.08 starts: toward the objective marker (the zone's far gate)
//  until it reads ZONE_1_START_DISTANCE plus ZONE_1_START_TOLERANCE or
//  less (walk_to_objective(), 60 s), then checks the end on a fresh frame:
//  the marker within the tolerance and the minimap's player arrow in the
//  zone's green (at_zone_1_start()). Throws OperationFailedException with
//  both readings when the end is not 03.08's start.
void walk_to_zone_1_start(ConsoleHandle& console, ProControllerContext& context);


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
