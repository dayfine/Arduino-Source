/*  Story Step JSON
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_StoryStepJson_H
#define PokemonAutomation_StoryStepJson_H
#include <string_view>
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon_StorySteps.h"
namespace PokemonAutomation{
namespace Pokemon{
//  Lossless tile-game pose data, including uncertainty and absent position/facing.
//  An unsupported Position subtype fails instead of being converted to unknown.
JsonValue story_pose_json(const Pose& pose);
//  Read a tile-game pose; malformed fields and unsupported position tags fail.
Pose read_story_pose(const JsonValue& value, std::string_view source);
//  Encode every shared step field. This does not bind or execute an action.
JsonValue story_step_json(const StoryStep& step);
//  Read the shared step record; unknown fields/kinds and invalid values fail.
StoryStep read_story_step(const JsonValue& value, std::string_view source);
//  Encode the common checkpoint; game adapters supply the shared policy/state envelope.
JsonValue story_checkpoint_json(const StoryCheckpoint& checkpoint);
//  Read only the common checkpoint record, retaining ordered steps and start belief.
StoryCheckpoint read_story_checkpoint(const JsonValue& value, std::string_view source);
} // namespace Pokemon
} // namespace PokemonAutomation
#endif
