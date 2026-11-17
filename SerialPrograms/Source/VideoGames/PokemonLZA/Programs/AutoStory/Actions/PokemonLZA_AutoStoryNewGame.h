/*  PokemonLZA AutoStory - New game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.00's steps before "Confirm Choices": launch the game from HOME, pick
 *  the language, the look and type the name (PokemonLZA_AutoStoryNewGameChoices.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryNewGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryNewGame_H

#include <optional>
#include <string>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonLZA_AutoStoryNewGameChoices.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The screens a new game (no save) may start on.
enum class NewGameStart{
    HOME,               //  The Switch's HOME (launch_plza_from_home() closes any game and finds PLZA's tile).
    LANGUAGE_LIST,
    CONFIRM_CHOICES,    //  The language, look and name chosen already.
};

//  The screen now, read over up to 2 s with no press. Throws UserSetupError,
//  having pressed nothing, if it is none of NewGameStart's.
NewGameStart require_new_game_start(ConsoleHandle& console, ProControllerContext& context);

//  Why `choices` cannot be played from `start`; nullopt when they can. From
//  HOME or the language list the program types the name
//  (name_typing_problem()); on Confirm Choices it is typed already.
std::optional<std::string> new_game_start_problem(NewGameStart start, const NewGameChoices& choices);

//  01.00's first step: from HOME (launch_plza_from_home()) or the language list, plays the language, the look and
//  the name of `choices` up to "Confirm Choices"; on "Confirm Choices" it
//  presses nothing. Each screen's highlight is read before and after each
//  press; the name is typed with the Switch keyboard typer. Throws
//  UserSetupError before any press if `choices` cannot be played
//  (new_game_problem(), new_game_start_problem()) or the keyboard's counter takes fewer characters
//  than the name, OperationFailedException (with a screenshot) if a screen
//  does not show, its highlight does not reach the choice, or a key press
//  does not reach the name's field.
void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
