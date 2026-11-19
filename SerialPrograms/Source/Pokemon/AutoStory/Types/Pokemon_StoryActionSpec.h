/*  Story Action Spec
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What an action declares: its step kind, name and args.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryActionSpec_H
#define PokemonAutomation_Pokemon_StoryActionSpec_H

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon_StoryActionArg.h"
#include "Pokemon_StoryStepKind.h"

namespace PokemonAutomation{
namespace Pokemon{


//  An arg kind a game registers (StoryArgType::REGISTERED). `read` throws
//  ParseException on a malformed value. Must outlive the specs naming it.
struct StoryArgKind{
    std::string name = {};
    std::function<StoryRegisteredArgValue(const JsonValue& value, std::string_view source)> read = {};
};

//  One argument an action takes. A missing optional one reads as `fallback`
//  when it has one.
struct StoryArgSpec{
    std::string key = {};
    StoryArgType type = StoryArgType::WORD;
    bool required = false;
    std::optional<StoryArgValue> fallback = {};
    //  What it is for, shown in lint errors.
    std::string why = {};
    //  For a REGISTERED arg: its kind, which reads it.
    const StoryArgKind* kind = nullptr;
};

//  An action: the kind of step it does, its name and the args it takes.
struct StoryActionSpec{
    StoryStepKind kind = StoryStepKind::GAME;
    std::string name = {};
    std::vector<StoryArgSpec> args = {};
    //  It runs only on a console backend.
    bool console_only = false;
};

//  Each way `args` don't fit `spec`: a key it doesn't name, a value of
//  another type, a required one missing. Empty when they fit.
std::vector<std::string> story_args_errors(const StoryActionSpec& spec, const StoryActionArgs& args);

//  `args` with each missing optional arg of `spec` set to its fallback.
StoryActionArgs with_fallbacks(const StoryActionSpec& spec, StoryActionArgs args);

//  The action of `kind` named `name` among `actions`; for an empty name, the
//  kind's first. Null when there is none.
const StoryActionSpec* find_story_action(std::span<const StoryActionSpec> actions, StoryStepKind kind, std::string_view name);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
