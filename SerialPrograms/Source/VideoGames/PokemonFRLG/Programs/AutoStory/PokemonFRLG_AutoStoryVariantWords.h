/*  AutoStory Variant Words
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading a GAME step's variant: words split on spaces, "key=value"
 *  pairs, and small numbers. The table writes variants such as
 *  "mart PewterCity_Mart budget=2000 potion=5"; the steps read them back.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryVariantWords_H
#define PokemonAutomation_PokemonFRLG_AutoStoryVariantWords_H

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  `text`'s words, split on spaces, empty words dropped.
std::vector<std::string_view> variant_words(std::string_view text);

//  A word "key=value" as its two halves; nothing without an '='.
struct VariantPair{
    std::string_view key;
    std::string_view value;
};
//  Splits a variant word into a key-value pair separated by "=".
std::optional<VariantPair> variant_pair(std::string_view word);

//  `text` as a number of up to `max_digits` decimal digits; nothing if it
//  isn't one.
std::optional<size_t> variant_number(std::string_view text, size_t max_digits = 6);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
