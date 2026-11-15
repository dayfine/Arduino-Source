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

//  03.07d's last step, from just inside Wild Zone 1's entrance gate to
//  where 03.08 starts: about 2.5 degrees left of the objective marker (the
//  zone's far gate, held at ZONE_1_START_AIM_X on screen) until it reads
//  ZONE_1_START_DISTANCE + ZONE_1_START_OVERSHOOT or less (60 s), so the
//  walk ends on 03.08's start frame's 127 rather than 2 past it; turns the
//  camera back onto the marker, then checks the end on a fresh frame:
//  the marker within the tolerance and the minimap's player arrow in the
//  zone's green (at_zone_1_start()). Throws OperationFailedException with
//  both readings when the end is not 03.08's start.
void walk_to_zone_1_start(ConsoleHandle& console, ProControllerContext& context);

//  After A at a wild zone's gate on the way out: the minimap's player arrow
//  out of the zone's green (MinimapZoneDetector) on one of 4 frames 0.5 s
//  apart. An A the game dropped leaves the player at the gate inside the
//  zone with the step after it passing on the prompt and party bar (the
//  fault sweep's lost tap). Throws OperationFailedException when the arrow
//  stays in the green.
void check_out_of_wild_zone(ConsoleHandle& console, ProControllerContext& context);


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
