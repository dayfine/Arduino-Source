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


//  Read a grid pose, {"map": "PalletTown", "tiles": [[3, 4]], "facing": "north"};
//  malformed and unknown fields fail. No "tiles" reads
//  as none (any tile, or a game with no tiles).
GridPose read_grid_pose(const JsonValue& value, std::string_view source);

//  Read a declared pose (a checkpoint's "start", a step's "declared_after"):
//  a tile one as a grid pose, {"map": ..., "tiles": [[x, y]], "facing": ...},
//  or a zone one, {"place": "LumioseCity", "spot": "HotelZ", "heading": 90}
//  (spot and heading optional). One with both or neither of "map" and "place",
//  and malformed or unknown fields, fail.
DeclaredPose read_declared_pose(const JsonValue& value, std::string_view source);


//  Read a gate change, {"opened": ["SilphCo_5F/door_1"], "switches": {"mansion": true}}
//  (the gates a step opens, the switches it sets; both optional, unknown
//  fields fail) and write it back.
GateChange read_gate_change(const JsonValue& value, std::string_view source);
//  The change as JSON, as read_gate_change() reads it.
JsonValue write_gate_change(const GateChange& change);

//  The same as a state: what a checkpoint's "start_gates" says is open and
//  on. "switches" that are off need not be listed, and are not written.
GateState read_gate_state(const JsonValue& value, std::string_view source);
//  The state as JSON, as read_gate_state() reads it.
JsonValue write_gate_state(const GateState& state);


//  Read a step, its args parsed against its action among `actions` (by
//  default the built-in ones; a game passes them with its own) and their
//  fallbacks filled in. Unknown fields, kinds, actions and args, mistyped
//  values and missing required args fail.
StoryStep read_story_step(
    const JsonValue& value, std::string_view source,
    std::span<const StoryActionSpec> actions = builtin_story_actions()
);


//  Read the common checkpoint, its steps read as read_story_step() does.
//  Its steps_json keeps the "events" array as read, dumped compactly.
StoryCheckpoint read_story_checkpoint(
    const JsonValue& value, std::string_view source,
    std::span<const StoryActionSpec> actions = builtin_story_actions()
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
