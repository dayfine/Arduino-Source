/*  PokemonLZA AutoStory - New game intro
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The new game's opening screens (language list, look picker, name
 *  keyboard): the presses each choice takes as pure functions; the language
 *  list's are in PokemonLZA_AutoStoryLanguageList.h. The screens'
 *  detectors are in PokemonLZA_AutoStoryIntroScreens.h.
 *
 *  The language list is as seen on console B (2026-11-13). The rest is a
 *  guess from Scarlet/Violet's, to be confirmed on a console (see the
 *  TODOs): the look picker's 2 x 4 grid, and what the name keyboard does
 *  with Shift.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryIntro_H
#define PokemonAutomation_PokemonLZA_AutoStoryIntro_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  One press on an intro screen.
enum class IntroKey{
    UP,
    DOWN,
    LEFT,
    RIGHT,
    A,
};


//  The number of looks on the picker: a grid of 2 rows of 4 faces, numbered
//  1 to LOOK_COUNT left to right along the top row, then the bottom row
//  (console B, 2026-11-13).
constexpr size_t LOOK_COUNT = 8;

//  The presses that move the picker's highlight from look `from` to look
//  `to`: LEFT or RIGHT to the column, then UP or DOWN to the row. Empty when
//  `from` is `to`; nullopt for a look outside 1 to LOOK_COUNT.
std::optional<std::vector<IntroKey>> look_picker_moves(size_t from, size_t to);


//  What to type for a player name on the keyboard. The keyboard types lower
//  case; its Shift (a left stick click) makes the next letter a capital.
struct NameKeys{
    //  Upper case letters and digits, the keys to press in order.
    std::string keys;
    //  Click Shift before the first key.
    bool shift_first = false;
};

//  Only the first letter of a name may be a capital: its Shift is the only
//  one the typer presses. `first_letter_auto_caps`: the keyboard starts with
//  a capital by itself (TODO: confirm on a console), so Shift is for a
//  first letter that is lower case. Nullopt for an empty name, a character
//  that is not a letter or digit, or a capital after the first letter.
std::optional<NameKeys> name_keys(std::string_view name, bool first_letter_auto_caps);


//  The new game's choices, played by the intro's actions.
struct NewGameChoices{
    //  Letters and digits, a capital only as the first letter (name_keys()).
    //  The lab's player is Kosmos.
    std::string name = "Kosmos";
    //  1 to LOOK_COUNT.
    size_t look = 1;
    //  The name keyboard starts with a capital by itself (name_keys()).
    bool name_first_letter_auto_caps = false;
};


//  Why the new game's choices cannot be played, or an empty string if they
//  can. The language is the list's.
std::string new_game_problem(Language language, const NewGameChoices& choices);


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
