/*  FRLG AutoStory - Console failure data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The fault files in ConsoleFailures/, built into the program
 *  (cmake/EmbedTextFiles.cmake): (file name, text), sorted by name.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStory_ConsoleFailureData_H
#define PokemonAutomation_PokemonFRLG_AutoStory_ConsoleFailureData_H

#include <string>
#include <utility>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  (file name without ".json", text) of each fault file.
const std::vector<std::pair<std::string, std::string>>& console_failure_json();


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
