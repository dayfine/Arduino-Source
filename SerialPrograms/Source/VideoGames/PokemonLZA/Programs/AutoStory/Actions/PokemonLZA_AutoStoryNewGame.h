/*  PokemonLZA AutoStory - New game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.00a's step, up to "Confirm Choices": launch the game from HOME, pick
 *  the language, the look and type the name. The choices and the presses
 *  each takes on its screen are pure functions, first below. The screens'
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
#include <vector>
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

//  One press on an intro screen.
enum class IntroKey{
    UP,
    DOWN,
    LEFT,
    RIGHT,
    A,
};

//  The new game's choices, from the program's options.
struct NewGameChoices{
    NewGameLanguage language = NewGameLanguage::English;
    //  1 to 8: left to right along the top row, then the bottom one.
    size_t look = 1;
    //  At most name_limit characters, all on the name keyboard's first page.
    std::string name = "Kosmos";
};

//  Why `choices` cannot be played; nullopt when they can.
std::optional<std::string> new_game_problem(const NewGameChoices& choices);

//  Column first, then row; nullopt for an entry outside the list.
std::optional<std::vector<IntroKey>> language_list_moves(size_t from, size_t to);

//  Column first, then row; nullopt for a look outside 1 to 8.
std::optional<std::vector<IntroKey>> look_picker_moves(size_t from, size_t to);



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
