/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Wild Zone 1's catches (03.07's tutorial, 03.08's five species), from
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


//  03.07's first catch (Fletchling): ZL locks on, ZL+ZR held 1.5 s throws
//  (0.5 s only aims). A break-out brings two or three lines from Taunie and
//  control back; the catch brings her lines and then the "Battling wild
//  Pokemon" cards (console B, 2026-11-10). So: lock with ZL held, choose
//  the nearest target and walk toward it when far (as catch_in_zone()
//  does), throw, A through her lines, again until a card shows, then A
//  through the cards. When a
//  break-out starts a wild battle, the rest of the throws go after
//  `battle_ai`'s attacks (weaken_then_throw). Throws
//  OperationFailedException after `max_throws` throws with no card.
void catch_tutorial(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int max_throws
);

//  03.07's second catch (Bunnelby), after the cards: A past its "weaken
//  it" banner (D-pad up is ignored while it shows), D-pad up sends the
//  partner out (again, twice at most per round, after two lock-ons with no
//  move symbols), then rounds of TrainerBattleState attacks (3 that land),
//  a throw and A through any lines (weaken_then_throw), until no target
//  locks on for a round: caught. Throws OperationFailedException after
//  `max_rounds`.
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
//  reads `stop_at` or less, or (with `stop_at` 0) an A prompt or a dialog
//  shows, or `limit`. Returns the last distance read.
std::optional<int> walk_to_objective(ConsoleHandle& console, ProControllerContext& context, int stop_at, Milliseconds limit);

//  03.08's catch for the "register five species" quest (4/5 after the
//  tutorial and its repeats): walks toward the objective (Wild Zone 1's far
//  gate, around Taunie's rocks from the start) to 60, where Mareep graze in
//  the grass right of the road, then patrols back to 100 and forward to 60
//  again (stops every 20, 2 laps, 8 minutes at most). At each stop it locks
//  on (in front or after small camera turns right; a step into the grass
//  when nothing does), cycles the targets with ZL held, walks toward the
//  nearest one not caught when it is far, and throws without letting ZL
//  go, counting the Pokedex's "Registered!" banners. At 58 from the road,
//  throws at a far Mareep fell short (console B, 2026-11-13). A break-out
//  that starts a wild battle (Mareep turns on the player, console B
//  2026-11-13) gets `battle_ai`'s attacks before the next throws. When
//  only species already caught lock on (a rerun from a save after the
//  catch), the quest is taken as done. Otherwise throws
//  OperationFailedException with fewer than `new_species_wanted` new
//  species after the patrol or `max_throws`.
void catch_in_zone(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int new_species_wanted, int max_throws
);

//  On to the objective marker until an A prompt (a gate's "Leave", a door's
//  "Enter") or a dialog (a scene that starts on arriving) shows. Where a
//  walk starts facing whichever way the last scene left the player (out of
//  a wild zone's gate), the marker sets the way, not timed legs (03.08,
//  console B 2026-11-10). Throws OperationFailedException when neither
//  shows within 90 s.
void walk_to_marker(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
