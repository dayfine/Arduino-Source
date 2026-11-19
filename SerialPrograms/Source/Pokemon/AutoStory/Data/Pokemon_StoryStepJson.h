/*  Story Step JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Strict readers of the story data types' shared JSON form.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStepJson_H
#define PokemonAutomation_Pokemon_StoryStepJson_H

#include <span>
#include <string_view>
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The checkpoint `value`, each step's args checked against `actions` with
//  fallbacks filled in; throws ParseException naming `source` on anything
//  unknown, mistyped or missing. "start" and "metadata" are notes for the
//  plan's readers and are not read.
StoryCheckpoint read_story_checkpoint(
    const JsonValue& value, std::string_view source, std::span<const StoryActionSpec> actions
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
