/*  Lab Request
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The text formats of a command file and its journal, shared by the
 *  Command File Runner and the lab channel. Nothing here touches the app,
 *  so all of it is unit tested.
 *
 */

#ifndef PokemonAutomation_LabRequest_H
#define PokemonAutomation_LabRequest_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Filesystem/Filesystem.h"

namespace PokemonAutomation{


//  The words of one command line. A word starting with '#' starts a comment
//  that runs to the end of the line. Word 0 is the command name.
std::vector<std::string> split_command_line(const std::string& line);

//  The ".txt" file in `inbox` whose name sorts first, if any.
std::optional<Filesystem::Path> next_request_file(const std::string& inbox);


//  Journal lines of a request's ".out" file. Times are from the start of the request.
std::string journal_ok(size_t number, const std::string& command, Milliseconds start, Milliseconds end);
std::string journal_error(size_t number, const std::string& command, const std::string& reason);





}
#endif
