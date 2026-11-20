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

#include <cstddef>
#include <optional>
#include <string_view>
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Waits for free control: two looks in a row with a sign of control and no
//  advance arrow, dialog box or fade (the end of a scene is not control).
//  Fails as "walk_not_free" when none comes within 30 s.
std::optional<Pokemon::StoryFailure> check_can_walk(PlzaBackend& backend, std::string_view walk);

//  One look, which on a console waits for the legs' inputs to run, so the
//  step ends where the walk does, not when its inputs are queued.
void finish_walk(PlzaBackend& backend, std::string_view walk);

//  Walks `move`, holding its last leg in slices until a look shows `end` or
//  a black fade. An A prompt ends it only when it shows again 300 ms later.
//  `end` seen before `end_after` of the last leg is passed on the way (an
//  NPC's prompt beside the path). With no `end`, the walk ends when a scene
//  takes control. Fails as "walk_no_end" / "walk_no_scene".
std::optional<Pokemon::StoryFailure> walk_until(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, std::optional<ScreenEvent> end, Milliseconds end_after
);

//  Walks `move` into a talk: A on any look of an A prompt during or up to
//  1 s after the last leg (a prompt at the edge of reach shows on single
//  looks, 01.03 on console B 2026-11-19), and again while it stays with
//  nothing open; walks on when the prompt goes with nothing started. A
//  straight one-leg walk with the prompt already up does not walk. Done
//  once a scene is under way; nothing is pressed with no prompt up. Fails
//  as "walk_no_end" when no talk starts, "talk_no_scene" when the prompt
//  stays through the As.
//  `after_legs`: A only once the legs are done, for a walk measured to end
//  facing its target past other prompts: in 01.03 a prompt mid-walk was
//  Taunie's, and A on it took her line, not Tepig's (console B, 2026-11-19).
std::optional<Pokemon::StoryFailure> walk_into_talk(PlzaBackend& backend, std::string_view walk, const StoryMove& move, bool after_legs);

//  A walk to an A prompt can stop beside its target at run pace (03.03b's
//  ladder, 2026-11-19). Steps sideways, left first,
//  then back past the start to the right, facing `facing_degrees` from the
//  camera again after each step, until a look shows the A prompt. The
//  steps taken; none when no prompt showed.
std::optional<size_t> step_along_until_prompt(PlzaBackend& backend, double facing_degrees);

//  A at a door's prompt, again while it stays with no fade or text. The
//  party bar shows under the prompt, so a scene machine run right after
//  would take it for control before a late fade.
void press_through_prompt(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
