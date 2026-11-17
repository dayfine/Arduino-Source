/*  PokemonLZA AutoStory - Walk screen checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a story walk (run_story_move(), PokemonLZA_AutoStoryMoves.h) reads
 *  of the screen, since the walk itself reads nothing: before it, that the
 *  player has free control (check_can_walk()); during its last leg, the
 *  event it ends on (walk_until()); after it, where it stopped
 *  (finish_walk()).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryWalkScreenChecks_H
#define PokemonAutomation_PokemonLZA_AutoStoryWalkScreenChecks_H

#include <string_view>
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Waits for free control: two looks in a row 250 ms apart with a sign of
//  control (the party bar, the station platform, the controls hint) and no advance arrow,
//  dialog box or fade. The end of a scene is not control: 01.01's scenes
//  end ~7 s before it. An A prompt held a second with none of those is
//  control too (01.03 has no party, so no party bar). Where no sign shows,
//  10 s with none of those is taken as control. Fails as
//  "walk_not_free" when neither comes
//  within 30 s: a walk started anywhere else walks blind.
Pokemon::Outcome check_can_walk(PlzaBackend& backend, std::string_view walk);

//  After a walk's legs with no end event: one look, which on a console
//  waits for the legs' inputs to run (observe()), so the step ends where
//  the walk does, not when its inputs are queued. Logs what the screen
//  shows there.
void finish_walk(PlzaBackend& backend, std::string_view walk);

//  Walks `move`, holding its last leg in slices of 250 ms
//  (hold_last_leg()) until a look shows `end` (a fade, the box of a talk
//  the walk runs into, or the A prompt of what it walks to), the leg's
//  time as the upper bound. An A prompt ends it only when it shows again
//  on a look 300 ms later, the stick let go; else the leg goes on. A black fade ends it too: the scene at the
//  walk's end takes over. A last leg that only turns is looked at once,
//  after the turn. Fails as "walk_no_end" if neither shows within it.
//  `end` seen before `end_after` of the last leg (the plan's measured walk)
//  does not end it: an NPC's A prompt beside the path is passed on the
//  way, not the one the walk goes to (02.05's Alton, console B 2026-11-16).
Pokemon::Outcome walk_until(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, ScreenEvent end, Milliseconds end_after
);

//  Walks `move` as walk_until() does, its last leg held in slices of
//  250 ms, until a scene takes control: something no walk may start on (a
//  text box, text waiting for A, a fade) on two looks in a row. The stick
//  is let go after each slice, so it is let go when the walk stops. A
//  walk that runs off into a scene with no measured end (03.03's drop off
//  the roof, console B 2026-11-17). Fails as "walk_no_scene" when control
//  is still free at the end of the last leg.
Pokemon::Outcome walk_until_scene(PlzaBackend& backend, std::string_view walk, const StoryMove& move);

//  Walks straight on, 250 ms at a time, until an A prompt shows
//  (INTERACT_PROMPT: "A Leave" at the Battle Zone's gate) and shows again
//  on a look 300 ms later, the stick let go; nothing if it shows already.
//  Fails as "no_prompt" after `timeout` of walking without one.
Pokemon::Outcome walk_to_prompt(PlzaBackend& backend, Milliseconds timeout);

//  Whether a look shows `event` within `within`, looking every 250 ms and
//  pressing nothing: "talk" presses A only on an A prompt
//  (INTERACT_PROMPT), "story_battle" fights only on the battle HUD
//  (BATTLE_STARTED).
bool event_shows_within(PlzaBackend& backend, ScreenEvent event, Milliseconds within);

//  "talk": on the A prompt of what the player faces (two looks in a row),
//  A, then its scene under way (scene_under_way(): Taunie's opens on a
//  card, 03.02b) on two looks in a row within 5 s, not one look of the
//  press itself. While the prompt stays up with nothing open, A again
//  every second, 3 As at most: the game can drop an A (02.05's elevator,
//  console B 2026-11-16, then read the overworld 120 s). No prompt on two
//  looks in a row within 3 s (it can come up seconds after the walk to it
//  ends: 02.05's elevator in German, 2026-11-16): nothing pressed,
//  "talk_no_prompt" (the walk stopped short, 03.02, 2026-11-16); nothing
//  after the As: "talk_no_scene".
Pokemon::Outcome talk_at_prompt(PlzaBackend& backend);

//  A at the A prompt the player stands at (a door's "A Enter"), then looks
//  for up to 3 s for a fade or text; while the prompt stays with neither,
//  A again, 3 times at most. The party bar shows under the prompt, so a
//  scene machine run right after would take it for control before a late
//  fade. A prompt gone with no fade or text, or still up after the last A,
//  is left to what runs next.
void press_through_prompt(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
