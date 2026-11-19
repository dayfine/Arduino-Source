/*  PokemonLZA AutoStory - New game choices
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The new game's choices (language, look, name) and the presses each takes
 *  on its screen, as pure functions. The screens' readers are in
 *  Detectors/PokemonLZA_AutoStoryIntroScreens.h; the name is typed with the
 *  Switch keyboard typer (NintendoSwitch_SystemKeyboardTyping.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryNewGameChoices_H
#define PokemonAutomation_PokemonLZA_AutoStoryNewGameChoices_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/Language.h"

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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
