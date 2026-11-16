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


//  The game's 10 languages, in the language list's order: down the left
//  column (English, Espanol (Espana), Espanol (Latinoamerica), Francais,
//  Deutsch), then down the right one (Italiano, Japanese, Korean,
//  Simplified and Traditional Chinese). The cursor starts on English.
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
    //  Short name for logs and capture files: "en", "es-ES", "ja", ...
    std::string_view tag;
    //  Its entry on the language list (the order of NewGameLanguage).
    size_t list_entry = 0;
    //  The name keyboard's character limit: its "0/N" counter (the
    //  console's frames of 2026-11-15: 12 for English and the other
    //  European languages, 6 for Japanese, Korean and Chinese).
    size_t name_limit = 0;
    //  The OCR language that reads its name on Confirm Choices.
    Language ocr_language = Language::None;
    //  A part of its name as Confirm Choices shows it ("Deutsch", "Latino").
    std::string_view confirm_text;
    //  The program can play it (Korean: not yet; its keyboard is Hangul,
    //  which the Switch keyboard typer does not read).
    bool supported = true;
};

//  `language`'s row of the table above.
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
    //  1 to 8: the look picker's portraits, left to right along the top
    //  row, then the bottom one.
    size_t look = 1;
    //  Letters, digits and the symbols on the name keyboard's first page
    //  (- / : ' , . ? !), at most the language's name_limit.
    std::string name = "Kosmos";
};

//  Why `choices` cannot be played, before any press; nullopt when they can.
std::optional<std::string> new_game_problem(const NewGameChoices& choices);

//  The presses that move the language list's cursor from entry `from` to
//  entry `to`: LEFT or RIGHT to the column, then UP or DOWN to the row.
//  Nullopt for an entry outside the list.
std::optional<std::vector<IntroKey>> language_list_moves(size_t from, size_t to);

//  The presses (stick tilts) that move the look picker's highlight from
//  look `from` to look `to` (1 to 8): LEFT or RIGHT, then UP or DOWN.
//  Nullopt for a look outside 1 to 8.
std::optional<std::vector<IntroKey>> look_picker_moves(size_t from, size_t to);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
