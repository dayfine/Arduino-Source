/*  Story Variant
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading a step's variant (StoryStep::variant): a name, then words split
 *  on spaces, some of them "key=value" pairs and small numbers. A table
 *  writes "mart PewterCity_Mart budget=2000 potion=5"; the name ("mart")
 *  picks the step's action (Pokemon_StoryActions.h) and the action reads
 *  the rest.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryVariant_H
#define PokemonAutomation_Pokemon_StoryVariant_H

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace PokemonAutomation{
namespace Pokemon{


//  `text`'s words, split on spaces, empty words dropped.
std::vector<std::string_view> variant_words(std::string_view text);

//  A word "key=value" as its two halves.
struct VariantPair{
    std::string_view key;
    std::string_view value;
};
//  `word` split at its first '='; nothing without one or with an empty key.
std::optional<VariantPair> variant_pair(std::string_view word);

//  The variant's name: its first word, unless that is a "key=value" pair.
//  Empty for an empty variant or one of pairs only.
std::string_view variant_name(std::string_view variant);

//  The value of the first "key=value" word with `key`; nothing if none.
std::optional<std::string_view> variant_value(std::string_view variant, std::string_view key);

//  `text` as a number of up to `max_digits` decimal digits; nothing if it
//  isn't one.
std::optional<size_t> variant_number(std::string_view text, size_t max_digits = 6);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
