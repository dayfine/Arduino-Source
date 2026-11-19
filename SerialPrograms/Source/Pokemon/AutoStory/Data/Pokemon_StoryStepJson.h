/*  Story Step JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads a story checkpoint's JSON into steps ready to run.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStepJson_H
#define PokemonAutomation_Pokemon_StoryStepJson_H

#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Parses a step's "args" into its typed args and returns the step's run
//  with them bound; throws ParseException on a bad arg.
using StoryActionParser = std::function<StoryStepRun(const JsonObject& args)>;

//  A game's actions by the name a step's "action" gives.
using StoryActions = std::map<std::string, StoryActionParser, std::less<>>;

//  Throws ParseException naming the first key of `object` not in `keys`.
void reject_unknown_keys(const JsonObject& object, std::initializer_list<std::string_view> keys);

//  The checkpoint `value`, each step parsed by its action among `actions`;
//  throws ParseException naming `source` on anything unknown, mistyped or
//  missing. "start" and "metadata" are notes for the plan's readers and are
//  not read.
StoryCheckpoint read_story_checkpoint(const JsonValue& value, std::string_view source, const StoryActions& actions);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
