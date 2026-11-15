/*  PokemonLZA AutoStory - Walks by the objective marker's distance
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks that keep the camera on the main mission's objective marker
 *  (read_objective_marker(), PokemonLZA_AutoStoryObjectiveMarkerReader.h)
 *  and stop at a distance it reads: 03.08's road stops back from it.
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

//  A ZL lock-on and throw leave the camera looking steeply down, and every
//  later sweep for brackets sees only the ground. Pins the pitch at its
//  lowest (the right stick down), then tips it up to the walking pitch
//  (horizon in the middle, far markers in the upper third).
void level_camera(ConsoleHandle& console, ProControllerContext& context);

//  Walks back away from the objective marker (the stick down: toward the
//  camera, turned back onto the marker before each step) until its distance reads `stop_at`
//  or more, or `limit`. A read more than 12 from the last is a misread, as
//  in walk_to_objective(). Returns the last distance read.
std::optional<int> walk_from_objective(ConsoleHandle& console, ProControllerContext& context, int stop_at, Milliseconds limit);

//  After A at a wild zone's gate on the way out: the minimap's player arrow
//  out of the zone's green (MinimapZoneDetector) on one of 4 looks, walking
//  on 600 ms away from the gate before each look after the first: just
//  outside the gate the arrow still touches the green's edge.
//  An A the game dropped leaves the player at the gate inside the
//  zone with the step after it passing on the prompt and party bar (the
//  fault sweep's lost tap). Throws OperationFailedException when the arrow
//  stays in the green.
void check_out_of_wild_zone(ConsoleHandle& console, ProControllerContext& context);


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
