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

//  A request file: an optional "owner <token>" line first, then commands.
//  Blank lines and comments are dropped.
struct LabRequest{
    std::string owner;
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
    std::string owner;      //  The owner token, or "".
    std::string error;      //  The last error; "" once a request succeeds.
};
std::string lab_state_text(const LabState& state);


//  The owner lock: once claimed, requests that drive the console must carry
//  the token. The lock lives in memory, so it dies with the app.
class LabOwner{
public:
    const std::string& token() const{ return m_token; }

    //  Each returns an error message, or "" on success. `request_owner` is the
    //  token on the request's "owner" line, "" if it has none.
    std::string claim(const std::string& request_owner, const std::string& token);
    std::string release(const std::string& request_owner);
    std::string check(const std::string& request_owner) const;

private:
    std::string m_token;
};


//  "keepawake on|off [seconds]": while no program runs, press a button every
//  `interval` so the console does not sleep.
class KeepAwakeSchedule{
public:
    static constexpr Seconds DEFAULT_INTERVAL = Seconds(15);

    bool on() const{ return m_on; }
    Seconds interval() const{ return m_interval; }

    //  Reads the words of a "keepawake" line. Returns an error message, or "".
    std::string set(const std::vector<std::string>& words, WallClock now);

    //  Whether a press is due now. Marks it done when it is.
    bool due(WallClock now, bool program_running);

private:
    bool m_on = false;
    Seconds m_interval = DEFAULT_INTERVAL;
    WallClock m_last_press = WallClock::min();
};



}
#endif
