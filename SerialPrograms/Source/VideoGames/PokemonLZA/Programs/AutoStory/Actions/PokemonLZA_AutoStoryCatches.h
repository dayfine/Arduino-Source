/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Wild Zone 1's catches (03.07's tutorial, 03.08's five species), from
 * .
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatches_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatches_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  03.07's first catch (Fletchling): ZL locks on, ZL+ZR held 1.5 s throws
//  (0.5 s only aims). A break-out brings two or three lines from Taunie and
//  control back; the catch brings her lines and then the "Battling wild
//  Pokemon" cards. So: lock with ZL held, choose
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

//  Walks toward the objective marker (read_objective_marker()): turns the camera to keep it in the
//  middle (to find it when it is behind) and walks on, until its distance
//  reads `stop_at` or less, or (with `stop_at` 0) an A prompt or a dialog
//  shows, or `limit`. Returns the last distance read.
std::optional<int> walk_to_objective(ConsoleHandle& console, ProControllerContext& context, int stop_at, Milliseconds limit);

//  What catch_in_zone() is after: `new_species` new species caught, so
//  the Pokedex has `dex_species`, in `max_throws` at most; `checks` is the
//  run's check level.
struct ZoneCatchGoal{
    int new_species = 0;
    int dex_species = 0;
    int max_throws = 0;
    Pokemon::StoryCheckLevel checks = Pokemon::StoryCheckLevel::DEFAULT;
};

//  03.08's catches for the "register five species" quest (3/5 after the
//  tutorial): walks toward the objective (Wild Zone 1's far gate, around
//  Taunie's rocks from the start), then goes back and forth between the
//  user's two catch spots (CATCH_SPOTS): 100 from the objective
//  (Scatterbug on the plaza's road) and 60 (Mareep by the gate),
//  PATROL_VISITS visits, 10 minutes at most. At each stop
//  it fights a wild battle that is on (the Run prompt, or the alert eye of
//  a wild Pokemon attacking: one of a species already caught is fought
//  off, others weakened and caught), else watches for the lock-on
//  brackets the game draws around one wild Pokemon (6 s at most), turns
//  the camera to the tallest (the nearest), holds ZL until the target's
//  name plate shows the lock (3 s at most), cycles the targets with ZL held, walks toward
//  the nearest one not caught when it is far, and throws without letting
//  ZL go, counting the Pokedex's "Registered!" banners. A break-out that
//  starts a wild battle gets `battle_ai`'s attacks before the next throws.
//  The overworld party bar's filled slots (PartyBarDetector), read before
//  the first stop and after each stop that threw, end the patrol at
//  `goal.dex_species` (party_has_species()): every catch is a new species,
//  so the filled slots are the registered species, and the banner is
//  missed at times. When only species already caught lock on (a rerun
//  from a save after the catch), the quest is taken as done. Unread, the
//  Pokedex's count is read when the tally is not all banners, or at
//  THOROUGH and TESTING checks (read_dex_count()), and then decides
//  (enough_species()). Throws OperationFailedException, still in the zone,
//  when short of `goal.dex_species` in the party bar or the Pokedex or,
//  unread, of `goal.new_species` new species after the patrol or
//  `goal.max_throws`.
void catch_in_zone(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, const ZoneCatchGoal& goal
);

//  On to the objective marker until an A prompt (a gate's "Leave", a door's
//  "Enter") or a dialog (a scene that starts on arriving) shows. Where a
//  walk starts facing whichever way the last scene left the player (out of
//  a wild zone's gate), the marker sets the way, not timed legs (03.08,
//  the console). Throws OperationFailedException when neither
//  shows within 90 s.
void walk_to_marker(ConsoleHandle& console, ProControllerContext& context);

//  03.08, after the catches: back onto the road at the catch spots
//  (road_stops_to_gate()), ending at the one nearest the far gate (60),
//  facing the objective marker. From 60 the far gate's pillars are straight
//  down the road, some 5 s on; the marker is not on the gate but past it
//  and to its left, behind the zone's fence, so a walk at the marker was
//  held up at 36 and wandered off.
//  The walk through the gate is walk_to_zone_gate_entrance()'s, steered by
//  the pillars on screen. Throws OperationFailedException when a road stop
//  is not reached.
void walk_back_to_zone_road(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
