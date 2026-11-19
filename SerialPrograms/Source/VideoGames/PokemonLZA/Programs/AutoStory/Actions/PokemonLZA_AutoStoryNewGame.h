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


//  Ends every refusal and failure of 01.00a.
constexpr std::string_view NEW_GAME_BY_HAND_HINT =
    "Make the new game's choices by hand up to \"Confirm Choices\", then start the run at 01.00b.";

//  Throws UserSetupError if the Switch HOME is not on screen; presses at most one B to wake it.
void require_new_game_home(ConsoleHandle& console, ProControllerContext& context);

//  Why a run of `rows` cannot type `choices`' name (only 01.00a types it); nullopt when it can.
std::optional<std::string> new_game_run_problem(std::span<const Pokemon::StoryCheckpoint> rows, const NewGameChoices& choices);

//  01.00a: from HOME, plays `choices` and stops on "Confirm Choices", confirming nothing.
//  Throws UserSetupError, before any press, if `choices` cannot be played or HOME is not
//  on screen, or if the keyboard takes fewer characters than the name;
//  OperationFailedException if a screen does not show or a highlight does not reach the choice.
void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
