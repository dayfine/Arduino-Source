/*  PokemonLZA AutoStory - New game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.00a's step, up to "Confirm Choices": launch the game from HOME, pick
 *  the language, the look and type the name. The choices come first below;
 *  the presses each takes on its screen are internal. The screens'
 *  readers are in Detectors/PokemonLZA_AutoStoryIntroScreens.h; the name is
 *  typed with the Switch keyboard typer (NintendoSwitch_SystemKeyboardTyping.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryNewGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryNewGame_H

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  In the language list's order: down the left column, then the right one.
enum class NewGameLanguage{
    English,
    SpanishSpain,
    SpanishLatinAmerica,
    French,
    German,
    Italian,
    Japanese,
    Korean,
    ChineseSimplified,
    ChineseTraditional,
};

//  What the intro needs to know of one language.
struct NewGameLanguageInfo{
    //  For logs and capture files: "en", "es-ES", ...
    std::string_view tag;
    size_t list_entry = 0;
    //  The name keyboard's "0/N" counter.
    size_t name_limit = 0;
    //  Reads its name on Confirm Choices.
    Language ocr_language = Language::None;
    //  A part of its name as Confirm Choices shows it.
    std::string_view confirm_text;
    //  False for Korean: the Switch keyboard typer does not type Hangul.
    bool keyboard_supported = true;
};

//  `language`'s table row.
const NewGameLanguageInfo& new_game_language_info(NewGameLanguage language);

//  The new game's choices, from the program's options.
struct NewGameChoices{
    NewGameLanguage language = NewGameLanguage::English;
    //  1 to 8: left to right along the top row, then the bottom one.
    size_t look = 1;
    //  At most name_limit characters, all on the name keyboard's first page.
    std::string name = "Kosmos";
};




//  Throws UserSetupError if the Switch HOME is not on screen; presses at most one B to wake it.
void require_new_game_home(ConsoleHandle& console, ProControllerContext& context);

//  Why a run of `rows` cannot play `choices`: a look or name the game refuses, or
//  a name 01.00a cannot type (only it types the name); nullopt when it can.
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
