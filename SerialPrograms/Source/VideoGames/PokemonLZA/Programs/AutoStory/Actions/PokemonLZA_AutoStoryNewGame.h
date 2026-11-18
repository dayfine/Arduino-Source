/*  PokemonLZA AutoStory - New game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.00a's step, up to "Confirm Choices": launch the game from HOME, pick
 *  the language, the look and type the name (PokemonLZA_AutoStoryNewGameChoices.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryNewGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryNewGame_H

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryNewGameChoices.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The end of every refusal of 01.00a and of its failures: a user whose
//  choices the program cannot make makes them by hand and starts at 01.00b.
constexpr std::string_view NEW_GAME_BY_HAND_HINT =
    "Make the new game's choices by hand up to \"Confirm Choices\", then start the run at 01.00b.";

//  Checks that the Switch HOME is on screen, read over up to 2 s with no
//  press. Throws UserSetupError, having pressed nothing, if it is not.
void require_new_game_home(ConsoleHandle& console, ProControllerContext& context);

//  Why a run of `rows` cannot play `choices`; nullopt when it can. A run
//  that plays the choices (story_row_makes_new_game_choices(): 01.00a)
//  types the name, so the language's keyboard and the name's characters
//  must be ones the program types; a run from 01.00b finds the name typed.
//  A refusal ends with NEW_GAME_BY_HAND_HINT.
std::optional<std::string> new_game_run_problem(std::span<const Pokemon::StoryCheckpoint> rows, const NewGameChoices& choices);

//  01.00a's step: from HOME (require_new_game_home(), then
//  launch_plza_from_home()), plays the language, the look and the name of
//  `choices` and ends on "Confirm Choices", confirming nothing. Each
//  screen's highlight is read before and after each press; the name is
//  typed with the Switch keyboard typer. Throws UserSetupError before any
//  press if `choices` cannot be played (new_game_problem(), or the name
//  cannot be typed) or HOME is not on screen, or when the keyboard's
//  counter takes fewer characters than the name;
//  OperationFailedException (with a screenshot) if a screen does not show,
//  its highlight does not reach the choice, or a key press does not reach
//  the name's field.
void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
