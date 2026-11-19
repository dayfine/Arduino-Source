/*  Shared Authored Checkpoint JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads an authored checkpoint file ("pokesim-authored-checkpoint/1").
 *
 */

#ifndef PokemonAutomation_Pokemon_AuthoredCheckpointJson_H
#define PokemonAutomation_Pokemon_AuthoredCheckpointJson_H

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon_StoryStepJson.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Parses an envelope's execution object; throws ParseException when it is
//  malformed.
using ExecutionReader = std::function<void(const JsonObject& execution, std::string_view source)>;

//  The checkpoint of `game`'s strict authored envelope, its steps parsed
//  by `actions` (read_story_checkpoint()); its execution object goes to
//  `read_execution`.
StoryCheckpoint read_authored_checkpoint(
    const JsonValue& value, std::string_view game, std::string_view source,
    const ExecutionReader& read_execution, const StoryActions& actions
);

//  A JSON file built into the program: its name without ".json", and its
//  text.
using EmbeddedJsonFile = std::pair<std::string, std::string>;



} // namespace Pokemon
} // namespace PokemonAutomation

#endif
