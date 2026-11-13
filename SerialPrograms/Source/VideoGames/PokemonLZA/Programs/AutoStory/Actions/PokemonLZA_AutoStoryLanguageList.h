/*  PokemonLZA AutoStory - New game language list
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The new game's language list: where each language is, and the presses
 *  between two entries, as pure functions. Its detector is in
 *  PokemonLZA_AutoStoryIntroScreens.h.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryLanguageList_H
#define PokemonAutomation_PokemonLZA_AutoStoryLanguageList_H

#include <optional>
#include <vector>
#include "CommonFramework/Language.h"
#include "PokemonLZA_AutoStoryIntro.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The language list (console B, 2026-11-13): 2 columns of 5 entries,
//  numbered down the left column, then down the right one:
//      0 English                   5 Italiano
//      1 Espanol (Espana)          6 Japanese
//      2 Espanol (Latinoamerica)   7 Korean
//      3 Francais                  8 Chinese Simplified
//      4 Deutsch                   9 Chinese Traditional
//  The cursor starts on English.

//  The rows in each column.
constexpr size_t LANGUAGE_LIST_ROWS = 5;

//  The entries: 2 columns of LANGUAGE_LIST_ROWS.
constexpr size_t LANGUAGE_LIST_SIZE = 2 * LANGUAGE_LIST_ROWS;

//  The list's entry for `language`. Language::Spanish is Espanol (Espana):
//  Language has no Latin American Spanish. Nullopt for a language the list
//  lacks.
std::optional<size_t> language_list_index(Language language);

//  The presses that move the cursor from entry `from` to entry `to`: LEFT
//  or RIGHT to the column, then UP or DOWN to the row, never round an edge.
//  Empty when `from` is `to`; nullopt for an entry outside the list.
std::optional<std::vector<IntroKey>> language_list_moves(size_t from, size_t to);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
