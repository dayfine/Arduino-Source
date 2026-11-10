/*  Shared Authored Checkpoint JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading and writing an authored checkpoint file: the shared
 *  StoryCheckpoint (Pokemon_StoryStepJson.h) in an envelope that names the
 *  game, plus the game's own execution object, which the game parses into
 *  its own type through a reader it passes in. Only parsing and encoding:
 *  nothing here binds or runs a step, and nothing read keeps JSON.
 *
 */

#ifndef PokemonAutomation_AuthoredCheckpointJson_H
#define PokemonAutomation_AuthoredCheckpointJson_H

#include <functional>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The game's reader of an envelope's execution object: it parses the
//  object into the game's own type and throws ParseException when it is
//  malformed.
using ExecutionReader = std::function<void(const JsonObject& execution, std::string_view source)>;

//  Require exactly these fields, including fields whose value is null/zero/false.
//  Missing/unknown fields throw ParseException with the document source.
void validate_json_fields(
    const JsonObject& object, std::initializer_list<std::string_view> fields, std::string_view source
);

//  The checkpoint of `game`'s strict authored envelope, its steps read
//  against `actions` (read_story_checkpoint()); its execution object goes
//  to `read_execution`.
StoryCheckpoint read_authored_checkpoint(
    const JsonValue& value, std::string_view game, std::string_view source,
    const ExecutionReader& read_execution,
    std::span<const StoryActionSpec> actions = builtin_story_actions()
);

//  Encode the authored envelope; execution is supplied by the game adapter.
//  This does not encode game bookkeeping or the legacy native-export envelope.
JsonValue authored_checkpoint_json(const StoryCheckpoint& checkpoint, std::string_view game, JsonObject execution);

//  A JSON file built into the program: its name without ".json", and its
//  text.
using EmbeddedJsonFile = std::pair<std::string, std::string>;

//  Find embedded file `file` among `files` and parse it as
//  read_authored_checkpoint() does. Missing resources throw
//  InternalProgramError; malformed documents throw ParseException.
StoryCheckpoint load_authored_checkpoint(
    std::span<const EmbeddedJsonFile> files, std::string_view file, std::string_view game,
    const ExecutionReader& read_execution,
    std::span<const StoryActionSpec> actions = builtin_story_actions()
);



} // namespace Pokemon
} // namespace PokemonAutomation

#endif
