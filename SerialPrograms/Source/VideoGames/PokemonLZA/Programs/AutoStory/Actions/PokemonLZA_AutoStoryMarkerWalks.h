/*  PokemonLZA AutoStory - Walks back from the objective marker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks that keep the camera on the main mission's objective marker
 *  (read_objective_marker(), PokemonLZA_AutoStoryObjectiveMarkerReader.h)
 *  and walk back
 *  away from it: 03.08's road stops, and 03.07d back from Wild
 *  Zone 1 to the Pokemon Center.
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

//  03.07d, from where 03.07's catches end in Wild Zone 1 to the
//  front of Vert's Pokemon Center, all walked back away from the objective
//  marker (the zone's far gate), the reverse of 03.08's center_to_zone_1():
//  to the entrance gate (the one 03.06 came in by) until its "A Leave" or
//  a dialog shows, A and B to the street, then on until the far gate reads
//  CENTER_FROM_FAR_GATE or 40 s. Throws OperationFailedException when no
//  prompt shows in 60 s, or unless the last read is out of the zone
//  (left_zone_1()), where 03.08 would take the player as in the zone and
//  skip its walk. UNVERIFIED, NEEDS CONSOLE: that the entrance and the
//  Center lie back from the marker with nothing in the way.
void zone_1_back_to_center(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
