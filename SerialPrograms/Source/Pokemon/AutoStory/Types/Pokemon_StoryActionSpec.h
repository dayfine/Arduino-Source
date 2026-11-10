/*  Story Action Spec
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What an action declares: the kind of step it does, its name and the
 *  args it takes, with their types and fallbacks (Pokemon_StoryActionArg.h).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryActionSpec_H
#define PokemonAutomation_Pokemon_StoryActionSpec_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon_StoryActionArg.h"
#include "Pokemon_StoryStepKind.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One argument an action takes. A missing optional one reads as `fallback`
//  when it has one.
struct StoryArgSpec{
    std::string key;
    StoryArgType type = StoryArgType::WORD;
    bool required = false;
    std::optional<StoryArgValue> fallback;
    //  What it is for, shown in lint errors.
    std::string why;
};

//  An action: the kind of step it does, its name and the args it takes.
struct StoryActionSpec{
    StoryStepKind kind = StoryStepKind::GAME;
    std::string name;
    std::vector<StoryArgSpec> args;
};

//  Each way `args` don't fit `spec`: a key it doesn't name, a value of
//  another type, a required one missing. Empty when they fit.
std::vector<std::string> story_args_errors(const StoryActionSpec& spec, const StoryActionArgs& args);

//  `args` with each missing optional arg of `spec` set to its fallback.
StoryActionArgs with_fallbacks(const StoryActionSpec& spec, StoryActionArgs args);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
