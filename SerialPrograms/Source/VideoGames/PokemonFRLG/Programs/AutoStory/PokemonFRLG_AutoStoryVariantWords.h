/*  AutoStory Variant Words
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading a GAME step: its variant names the action ("mart") and its
 *  args (StoryStep::args) hold what the action reads, checked against the
 *  action's specs. Also the old variant words ("key=value" pairs and small
 *  numbers), for the item slugs that carry digits ("tm28").
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryVariantWords_H
#define PokemonAutomation_PokemonFRLG_AutoStoryVariantWords_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_StoryArgs.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"

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


//  `step`'s args when its variant is `name` and they fit `specs`
//  (Pokemon::story_args_error()); nullptr otherwise.
const Pokemon::StoryArgs* game_step_args(
    const Pokemon::StoryStep& step, std::string_view name, std::span<const Pokemon::StoryArgSpec> specs
);

//  Whether `args` has no number `key`, or one from `min` to `max`.
bool arg_number_within(const Pokemon::StoryArgs& args, std::string_view key, int64_t min, int64_t max);

//  A GAME step written the old way, e.g. "teach hm03 surf slot=1": its
//  first word the variant, the rest its args (Pokemon::split_legacy_variant()),
//  the plain words after the name taking `positional`'s keys. One that
//  can't be split keeps `text` as its variant, which no action reads.
Pokemon::StoryStep legacy_game_step(std::string_view text, std::span<const std::string_view> positional = {});



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
