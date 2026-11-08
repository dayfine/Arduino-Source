/*  Authored FRLG Checkpoint Data
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_PokemonFRLG_AutoStoryData_H
#define PokemonAutomation_PokemonFRLG_AutoStoryData_H
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonFRLG_AutoStoryEvents.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
//  Read a data-authored checkpoint, retaining native bookkeeping defaults.
//  Unknown envelope fields/menu values and malformed common actions throw ParseException.
StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source);
//  Load an embedded authored plan by filename without .json; no controller IO.
StoryCheckpoint load_authored_checkpoint(std::string_view file);
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
