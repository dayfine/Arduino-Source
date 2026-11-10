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
 *  Deprecated: a step's variant is now a plain action name and its
 *  arguments are structured (StoryStep::args, Types/Pokemon_StoryArgs.h).
 *  split_legacy_variant() reads an old variant into that form while plans
 *  and callers move over; the word helpers go once none reads them.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryVariant_H
#define PokemonAutomation_Pokemon_StoryVariant_H

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Types/Pokemon_StoryArgs.h"

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


//  An old variant as an action name and its args, or why it can't be read.
struct LegacyVariant{
    std::string name;
    StoryArgs args;
    std::string error;
};

//  `variant` written the old way ("mart CeruleanCity_Mart budget=3000
//  if_present"): its first word is the name; the next plain words take the
//  keys in `positional`, in order, and plain words after those are flags;
//  "key=value" words are numbers when all digits, else words. It reads no
//  action's specs: check the args against them (story_args_error()).
LegacyVariant split_legacy_variant(std::string_view variant, std::span<const std::string_view> positional = {});



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
