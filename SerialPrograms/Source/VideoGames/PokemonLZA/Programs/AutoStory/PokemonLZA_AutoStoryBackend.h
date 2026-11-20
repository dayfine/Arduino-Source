/*  PokemonLZA AutoStory - Story backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's backend for the engine (Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h)
 *  and the turns every backend takes the same way. The console's backend is
 *  PlzaConsoleBackend (PokemonLZA_AutoStoryConsoleBackend.h).
 *
 *  Directions are the stick relative to the camera: north is straight on,
 *  east to the right. story_turn() turns that way; stick() walks.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackend_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackend_H

#include <functional>
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The engine's backend for PLZA's pose.
using PlzaBackend = Pokemon::StoryBackend<Pokemon::ZonePose>;

//  Turns the player `degrees` clockwise of straight on (negative: left),
//  as the story was recorded: L recenters the camera behind the player,
//  then the stick is tapped that way for 150 ms. The tap also takes a step.
void story_turn(PlzaBackend& backend, double degrees);

//  The player's heading on the minimap as its arrow shows it now
//  (upstream's get_facing_direction(): 0 up, 90 right).
using MinimapArrowReader = std::function<double()>;

//  Holds ZL up to `hold` while reading the battle menu's move-effectiveness
//  symbols (MoveEffectivenessSymbolDetector), the sign of a lock-on that
//  upstream's TrainerBattle and the catches take; true once they show.
using LockOnWatch = std::function<bool(Milliseconds hold)>;

//  Turns the player to `heading` on the minimap from any start, 4 turns
//  at most, each story_turn() by the arrow's error the short way round,
//  then L puts the camera behind the player, so a run forward goes that
//  way (console B, 2026-11-14). The arrow is read only once the last turn
//  is done (backend.observe() waits for it): in run 10 of 03.08 (console
//  B, 2026-11-16) it was read while the turns were still queued, read 132
//  four times, and failed. After a turn the arrow is read again until it
//  has moved at least half the turn (10 reads, 100 ms apart, at most): a
//  feed running behind the game shows the facing from before the turn on
//  the first reads. Fails as "heading_missed".
Pokemon::Outcome face_heading(PlzaBackend& backend, const MinimapArrowReader& read_arrow, double heading);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
