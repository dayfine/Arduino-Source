/*  Shared Authored Checkpoint JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading and writing an authored checkpoint file: the shared
 *  StoryCheckpoint (Pokemon_StoryStepJson.h) in an envelope that names the
 *  game, plus the game's own execution object, left as JSON for the game
 *  to read. Only parsing and encoding: nothing here binds or runs a step.
 *
 */

#ifndef PokemonAutomation_AuthoredCheckpointJson_H
#define PokemonAutomation_AuthoredCheckpointJson_H

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Common story data and an owned execution object for the game adapter to validate.
//  Parsing this document does not validate game policies or bind/execute actions.
struct AuthoredCheckpoint{
    StoryCheckpoint checkpoint;
    JsonObject execution;
};

//  Require exactly these fields, including fields whose value is null/zero/false.
//  Missing/unknown fields throw ParseException with the document source.
void validate_json_fields(
    const JsonObject& object, std::initializer_list<std::string_view> fields, std::string_view source
);

//  Read the strict authored envelope and common checkpoint of `game`.
//  The game adapter must still validate execution fields and values before use.
AuthoredCheckpoint read_authored_checkpoint(
    const JsonValue& value, std::string_view game, std::string_view source
);

//  Encode the authored envelope; execution is supplied by the game adapter.
//  This does not encode game bookkeeping or the legacy native-export envelope.
JsonValue authored_checkpoint_json(const StoryCheckpoint& checkpoint, std::string_view game, JsonObject execution);

//  A JSON file built into the program: its name without ".json", and its
//  text.
using EmbeddedJsonFile = std::pair<std::string, std::string>;

//  Find embedded file `file` among `files` and parse it as `game`'s
//  authored checkpoint. Missing resources throw InternalProgramError;
//  malformed documents throw ParseException.
AuthoredCheckpoint load_authored_checkpoint(
    std::span<const EmbeddedJsonFile> files, std::string_view file, std::string_view game
);



} // namespace Pokemon
} // namespace PokemonAutomation

#endif
