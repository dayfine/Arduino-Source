/*  AutoStory Trainer Pass
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A step across the line of a trainer who turns on a timer, on the turn
 *  away, as pokeguy passes Juggler Kayden in Fuchsia Gym.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTrainerPass_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTrainerPass_H

#include <functional>
#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The variant of a STEP across the line of a trainer who turns on a timer
//  (MOVEMENT_TYPE_FACE_DOWN_AND_RIGHT and its kin), as pokeguy passes
//  Juggler Kayden in Fuchsia Gym (1:15:14): from outside their sight, the
//  trainer (at "trainer_dx", "trainer_dy" tiles from the player) is watched,
//  up to TRAINER_PASS_WATCH, until they turn "away", and then the player
//  steps "tiles" tiles `direction` across the line. They hold a way at least
//  32 frames (pokefirered gMovementDelaysShort), longer than the step onto
//  the line takes, so the crossing starts on a turn seen
//  (wait_for_turn_away()), not on a facing that has lasted. If they see the
//  player anyway (no turn seen in time, or the facing misread), their
//  battle ("trainer", "foes") is fought there and the crossing goes on.
inline const std::string TRAINER_PASS_VARIANT = "trainer_pass";
//  The longest wait for the trainer's turn away.
inline constexpr Milliseconds TRAINER_PASS_WATCH(20000);
//  How often the trainer's facing is read while waiting.
inline constexpr Milliseconds TRAINER_PASS_POLL(50);

//  Reads `read` (the trainer's facing, or nothing if it can't be read),
//  then `wait` and again, at most `polls` times, until it reads `away`
//  after reading another way. Returns whether it did.
bool wait_for_turn_away(
    int polls, Pokemon::Direction away,
    const std::function<std::optional<Pokemon::Direction>()>& read, const std::function<void()>& wait
);

//  A TRAINER_PASS_VARIANT `step` on the console, from `state`'s pose;
//  `fight` fights the trainer's battle (a BATTLE step) if they see the
//  player. The line is one tile deep where the crossing meets it, its
//  first tile. No sprite of the trainer is in Resources yet to read their
//  facing on screen, so the crossing starts at once, logged.
Pokemon::Outcome pass_trainer(
    StoryEnv& env, const Pokemon::RunState& state, const StoryStep& step,
    const std::function<Pokemon::Outcome(const StoryStep&)>& fight
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
