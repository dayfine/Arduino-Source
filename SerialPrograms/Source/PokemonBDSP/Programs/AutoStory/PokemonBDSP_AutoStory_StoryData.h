/*  BDSP Authored Checkpoint Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryData_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryData_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Strict authored envelope and BDSP save/failure configuration; no controller IO.
//  Unknown fields/values or inconsistent save policy throw ParseException.
StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source);
//  The embedded authored JSON files, keyed by filename without .json.
const std::vector<std::pair<std::string, std::string>>& bdsp_checkpoint_json();
//  Load an embedded authored checkpoint; a missing file throws InternalProgramError.
StoryCheckpoint load_authored_checkpoint(std::string_view file);
} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
