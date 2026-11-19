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

#include <optional>
#include <string_view>
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Waits for free control: two looks in a row with a sign of control and no
//  advance arrow, dialog box or fade (the end of a scene is not control).
//  Fails as "walk_not_free" when none comes within 30 s.
Pokemon::Outcome check_can_walk(PlzaBackend& backend, std::string_view walk);

//  One look, which on a console waits for the legs' inputs to run, so the
//  step ends where the walk does, not when its inputs are queued.
void finish_walk(PlzaBackend& backend, std::string_view walk);

//  Walks `move`, holding its last leg in slices until a look shows `end` or
//  a black fade. An A prompt ends it only when it shows again 300 ms later.
//  `end` seen before `end_after` of the last leg is passed on the way (an
//  NPC's prompt beside the path). With no `end`, the walk ends when a scene
//  takes control. Fails as "walk_no_end" / "walk_no_scene".
Pokemon::Outcome walk_until(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, std::optional<ScreenEvent> end, Milliseconds end_after
);

//  "talk": A on the prompt of what the player faces, A again while it stays
//  up with nothing open (the game can drop an A). Fails as "talk_no_prompt"
//  with nothing pressed when no prompt settles within 3 s, "talk_no_scene"
//  when nothing follows the As.
Pokemon::Outcome talk_at_prompt(PlzaBackend& backend);

//  A at a door's prompt, again while it stays with no fade or text. The
//  party bar shows under the prompt, so a scene machine run right after
//  would take it for control before a late fade.
void press_through_prompt(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
