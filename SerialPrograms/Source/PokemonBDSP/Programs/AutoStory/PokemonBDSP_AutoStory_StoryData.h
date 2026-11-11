/*  BDSP Authored Checkpoint Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryData_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryData_H

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The action specifications that BDSP steps parse against.
std::span<const Pokemon::StoryActionSpec> bdsp_story_actions();

//  The embedded authored JSON files, keyed by filename without .json.
const std::vector<std::pair<std::string, std::string>>& bdsp_checkpoint_json();

//  Strict authored envelope and BDSP save/failure configuration; no controller IO.
//  Unknown fields/values or inconsistent save policy throw ParseException.
StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source);

//  Load an embedded authored checkpoint; a missing file throws InternalProgramError.
StoryCheckpoint load_authored_checkpoint(std::string_view file);

//  Load all embedded authored checkpoints in id order.
std::vector<StoryCheckpoint> load_authored_story();


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
