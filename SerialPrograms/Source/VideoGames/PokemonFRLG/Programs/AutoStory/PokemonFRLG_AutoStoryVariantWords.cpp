/*  AutoStory Variant Words
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon/AutoStory/Pokemon_StoryVariant.h"
#include "PokemonFRLG_AutoStoryVariantWords.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::vector<std::string_view> variant_words(std::string_view text){
    std::vector<std::string_view> words;
    while (!text.empty()){
        const size_t space = text.find(' ');
        const std::string_view word = text.substr(0, space);
        if (!word.empty()){
            words.push_back(word);
        }
        if (space == std::string_view::npos){
            break;
        }
        text.remove_prefix(space + 1);
    }
    return words;
}

std::optional<VariantPair> variant_pair(std::string_view word){
    const size_t equals = word.find('=');
    if (equals == std::string_view::npos || equals == 0){
        return std::nullopt;
    }
    return VariantPair{word.substr(0, equals), word.substr(equals + 1)};
}

std::optional<size_t> variant_number(std::string_view text, size_t max_digits){
    if (text.empty() || text.size() > max_digits){
        return std::nullopt;
    }
    size_t value = 0;
    for (char c : text){
        if (c < '0' || c > '9'){
            return std::nullopt;
        }
        value = value * 10 + size_t(c - '0');
    }
    return value;
}


const Pokemon::StoryArgs* game_step_args(
    const Pokemon::StoryStep& step, std::string_view name, std::span<const Pokemon::StoryArgSpec> specs
){
    if (step.variant != name || !Pokemon::story_args_error(step.args, specs).empty()){
        return nullptr;
    }
    return &step.args;
}

bool arg_number_within(const Pokemon::StoryArgs& args, std::string_view key, int64_t min, int64_t max){
    const std::optional<int64_t> value = Pokemon::story_arg_number(args, key);
    return !value || (*value >= min && *value <= max);
}

Pokemon::StoryStep legacy_game_step(std::string_view text, std::span<const std::string_view> positional){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    Pokemon::LegacyVariant split = Pokemon::split_legacy_variant(text, positional);
    if (!split.error.empty()){
        step.variant = std::string(text);
        return step;
    }
    step.variant = std::move(split.name);
    step.args = std::move(split.args);
    return step;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
