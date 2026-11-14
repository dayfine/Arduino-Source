/*  Story Step JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The shared JSON wire form of the story data types (Types/): poses,
 *  steps and checkpoints, read strictly and written losslessly. This is
 *  the only layer that reads story JSON: a step comes out with its args
 *  typed and checked, and nothing after it parses.
 *
 */

#ifndef PokemonAutomation_StoryStepJson_H
#define PokemonAutomation_StoryStepJson_H

#include <span>
#include <string_view>
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A grid pose: {"map": "PalletTown", "tiles": [[3, 4]], "facing": "north"}.
JsonValue grid_pose_json(const GridPose& pose);

//  Read a grid pose; malformed and unknown fields fail.
GridPose read_grid_pose(const JsonValue& value, std::string_view source);


//  Encode a step: its kind, action and args. This does not run it.
JsonValue story_step_json(const StoryStep& step);

//  Read a step, its args parsed against its action among `actions` (by
//  default the built-in ones; a game passes them with its own) and their
//  fallbacks filled in. Unknown fields, kinds, actions and args, mistyped
//  values and missing required args fail.
StoryStep read_story_step(
    const JsonValue& value, std::string_view source,
    std::span<const StoryActionSpec> actions = builtin_story_actions()
);


//  Encode the common checkpoint; game adapters supply the shared policy/state envelope.
JsonValue story_checkpoint_json(const StoryCheckpoint& checkpoint);

//  Read the common checkpoint, its steps read as read_story_step() does.
StoryCheckpoint read_story_checkpoint(
    const JsonValue& value, std::string_view source,
    std::span<const StoryActionSpec> actions = builtin_story_actions()
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
