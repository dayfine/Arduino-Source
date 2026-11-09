/*  Shared Authored Checkpoint JSON
 *  From: https://github.com/PokemonAutomation/
 */

#ifndef PokemonAutomation_AuthoredCheckpointJson_H
#define PokemonAutomation_AuthoredCheckpointJson_H

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon_StorySteps.h"

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
void require_story_json_fields(
    const JsonObject& object, std::initializer_list<std::string_view> fields, std::string_view source
);

//  Read the strict authored envelope and common checkpoint for expected_game.
//  The game adapter must still validate execution fields and values before use.
AuthoredCheckpoint read_authored_checkpoint(
    const JsonValue& value, std::string_view expected_game, std::string_view source
);

//  Encode the authored envelope; execution is supplied by the game adapter.
//  This does not encode game bookkeeping or the legacy native-export envelope.
JsonValue authored_checkpoint_json(const StoryCheckpoint& checkpoint, std::string_view game, JsonObject execution);

//  Find and parse an embedded (filename without .json, text) resource for this game.
//  Missing resources throw InternalProgramError; malformed documents throw ParseException.
AuthoredCheckpoint load_authored_checkpoint(
    std::span<const std::pair<std::string, std::string>> files, std::string_view file, std::string_view expected_game
);


} // namespace Pokemon
} // namespace PokemonAutomation

#endif
