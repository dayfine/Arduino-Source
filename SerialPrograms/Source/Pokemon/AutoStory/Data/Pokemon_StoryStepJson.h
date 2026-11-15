/*  Story Step JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Readers of the shared JSON wire form of the story data types (Types/):
 *  poses, steps and checkpoints, read strictly. This is
 *  the only layer that reads story JSON: a step comes out with its args
 *  typed and checked, and nothing after it parses.
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


//  Read a declared pose (a checkpoint's "start"): a zone one,
//  {"place": "LumioseCity", "spot": "HotelZ", "heading": 90} (spot and
//  heading optional). Malformed and unknown fields fail.
DeclaredPose read_declared_pose(const JsonValue& value, std::string_view source);


//  Read a step, its args parsed against its action among `actions` (the
//  game's) and their fallbacks filled in. Unknown fields, kinds, actions and args, mistyped
//  values and missing required args fail.
StoryStep read_story_step(
    const JsonValue& value, std::string_view source,
    std::span<const StoryActionSpec> actions
);


//  Read the common checkpoint, its steps read as read_story_step() does.
StoryCheckpoint read_story_checkpoint(
    const JsonValue& value, std::string_view source,
    std::span<const StoryActionSpec> actions
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
