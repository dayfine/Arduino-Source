/*  Lab Request
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The text formats of the lab channel (see LabChannel.h): a request file,
 *  its journal, the "state" file, the owner lock and the keep-awake schedule.
 *  Nothing here touches the app, so all of it is unit tested.
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


//  One command line of a request, with its line number for the journal.
struct LabRequestLine{
    size_t number;
    std::string text;
    std::vector<std::string> words;
};

//  A request file: its commands. Blank lines and comments are dropped.
struct LabRequest{
    std::vector<LabRequestLine> lines;
};
LabRequest parse_lab_request(const std::string& text);


//  Journal lines of a request's ".out" file. Times are from the start of the request.
std::string journal_ok(size_t number, const std::string& command, Milliseconds start, Milliseconds end);
std::string journal_error(size_t number, const std::string& command, const std::string& reason);


//  What the "state" file says.
struct LabState{
    std::string seq;        //  Name of the last request handled.
    std::string state;      //  STOPPED, RUNNING, STOPPING, NOT_READY or NO_PROGRAM.
    std::string program;    //  The selected program's identifier, or "".
    std::string error;      //  The last error; "" once a request succeeds.
};
std::string lab_state_text(const LabState& state);





}
#endif
