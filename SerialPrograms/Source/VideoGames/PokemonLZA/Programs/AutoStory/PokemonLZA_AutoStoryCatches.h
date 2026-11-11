/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Wild Zone 1's catches (03.06's tutorial, 03.07's five species), from
 *  console B, 2026-11-10.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatches_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatches_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  03.06's first catch (Fletchling): ZL locks on, ZL+ZR held 1.5 s throws
//  (0.5 s only aims). A break-out brings two or three lines from Taunie and
//  control back; the catch brings her lines and then the "Battling wild
//  Pokemon" cards (console B, 2026-11-10). So: lock, throw, A through her
//  lines, again until a card shows, then A through the cards. Throws
//  OperationFailedException after `max_throws` throws with no card.
void catch_tutorial(ConsoleHandle& console, ProControllerContext& context, int max_throws);

//  03.06's second catch (Bunnelby), after the cards: A past its "weaken
//  it" banner (D-pad up is ignored while it shows), D-pad up sends the
//  partner out (again, twice at most per round, after two lock-ons with no
//  move symbols), then rounds of TrainerBattleState attacks (3 that land),
//  a throw and A through any lines, until no target locks on for a round:
//  caught. Throws OperationFailedException after `max_rounds`.
void battle_then_catch(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int max_rounds
);

//  The main mission's objective marker in the overworld: an orange hexagon
//  "!" with the distance above it in white digits.
struct ObjectiveMarker{
    //  The hexagon's middle, 0 (left edge) to 1 (right edge).
    double x = 0;
    //  The digits read; -1 when they did not read.
    int distance = -1;
};

//  The marker on `screen`, if it shows (the minimap's own marker aside).
std::optional<ObjectiveMarker> read_objective_marker(Logger& logger, const ImageViewRGB32& screen);

//  Walks toward the objective marker: turns the camera to keep it in the
//  middle (to find it when it is behind) and walks on, until its distance
//  reads `stop_at` or less, or (with `stop_at` 0) an A prompt shows, or
//  `limit`. Returns the last distance read.
std::optional<int> walk_to_objective(ConsoleHandle& console, ProControllerContext& context, int stop_at, Milliseconds limit);

//  03.07's catches for the "register five species" quest (3/5 after the
//  tutorial), in Wild Zone 1: walks toward the objective (the zone's far
//  gate, 127 away at the start, where nothing spawns) and stops at 100 and
//  at 60 to throw at what locks on in front, or else what a camera turn
//  finds, up to 3 throws a stop. A throw followed by text or a card counts
//  as a catch. Throws OperationFailedException with fewer than
//  `catches_wanted` after both stops or `max_throws`.
void catch_in_zone(ConsoleHandle& console, ProControllerContext& context, int catches_wanted, int max_throws);

//  On to the objective until the gate's A prompt shows (03.07, Wild Zone
//  1's far gate). Throws OperationFailedException when no prompt shows.
void walk_to_zone_gate(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
