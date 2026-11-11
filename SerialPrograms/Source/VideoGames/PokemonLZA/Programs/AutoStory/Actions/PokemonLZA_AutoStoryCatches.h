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

//  03.07's catch for the "register five species" quest (4/5 after the
//  tutorial and its repeats): walks toward the objective (Wild Zone 1's far
//  gate, around Taunie's rocks from the start) to 60, where Mareep graze in
//  the grass right of the road, then steps right 1 s at a time (3 at most)
//  and throws at what locks on in front, or after small camera turns right,
//  counting the Pokedex's "Registered!" banners; then steps back onto the
//  road. When only species already caught lock on there (a rerun from a
//  save after the catch), the quest is taken as done. Otherwise throws
//  OperationFailedException with fewer than `new_species_wanted` new
//  species after the steps or `max_throws`.
void catch_in_zone(ConsoleHandle& console, ProControllerContext& context, int new_species_wanted, int max_throws);

//  On to the objective until the gate's A prompt shows (03.07, Wild Zone
//  1's far gate). Throws OperationFailedException when no prompt shows.
void walk_to_zone_gate(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
