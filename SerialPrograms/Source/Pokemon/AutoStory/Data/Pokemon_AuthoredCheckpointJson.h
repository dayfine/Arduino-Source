/*  Shared Authored Checkpoint JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading an authored checkpoint file: the shared
 *  StoryCheckpoint (Pokemon_StoryStepJson.h) in an envelope that names the
 *  game, plus the game's own execution object, which the game parses into
 *  its own type through a reader it passes in. Only parsing:
 *  nothing here binds or runs a step, and nothing read keeps JSON.
 *
 *  The envelope's "format" is "pokesim-authored-checkpoint/1": the
 *  files are written by PokemonSimulation's story tools, which plan
 *  and check a checkpoint's steps offline; a file written by hand uses the
 *  same name.
 *
 */

#ifndef PokemonAutomation_Pokemon_AuthoredCheckpointJson_H
#define PokemonAutomation_Pokemon_AuthoredCheckpointJson_H

#include <functional>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
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
    std::span<const StoryActionSpec> actions
);

//  A JSON file built into the program: its name without ".json", and its
//  text.
using EmbeddedJsonFile = std::pair<std::string, std::string>;



} // namespace Pokemon
} // namespace PokemonAutomation

#endif
