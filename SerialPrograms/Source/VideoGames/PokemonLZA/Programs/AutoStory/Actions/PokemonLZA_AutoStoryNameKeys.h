/*  PokemonLZA AutoStory - Name keys
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The player's name as presses on the Switch's name keyboard, a pure
 *  function on a model of its key grid (the console's frames in nine
 *  languages, 2026-11-13). Its first page has four rows of 11 keys:
 *
 *      1 2 3 4 5 6 7 8 9 0 -
 *      q w e r t y u i o p /
 *      a s d f g h j k l : '
 *      z x c v b n m , . ? !
 *
 *  and a bottom row (globe, Shift, the keyboard's modes, Space). The cursor
 *  starts on "1". Letters type lower case; Shift (a left stick click) makes
 *  the next letter a capital. Simplified Chinese opens in Pinyin mode: its
 *  bottom row's fourth key ("English") switches to letters first.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryNameKeys_H
#define PokemonAutomation_PokemonLZA_AutoStoryNameKeys_H

#include <optional>
#include <string_view>
#include <vector>
#include "PokemonLZA_AutoStoryNewGameChoices.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The presses that type `name` on the keyboard of `language` and press OK
//  (PLUS): per character D-pad moves from the last key (across, then up or
//  down; never round an edge), SHIFT before a capital, then A. Nullopt for
//  an empty name or a character with no key on the first page (a space
//  included).
std::optional<std::vector<IntroKey>> name_key_presses(std::string_view name, NewGameLanguage language);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
