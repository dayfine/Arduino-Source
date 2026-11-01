/*  Lab Request
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cerrno>
#include <cstdlib>
#include <format>
#include <sstream>
#include "LabRequest.h"

namespace PokemonAutomation{



std::vector<std::string> split_command_line(const std::string& line){
    std::vector<std::string> words;
    std::istringstream in(line);
    std::string word;
    while (in >> word && word[0] != '#'){
        words.push_back(word);
    }
    return words;
}

std::optional<Filesystem::Path> next_request_file(const std::string& inbox){
    std::optional<Filesystem::Path> next;
    for (const Filesystem::Path& path : Filesystem::list_directory(Filesystem::Path(inbox))){
        if (!Filesystem::is_regular_file(path) || path.extension().string() != ".txt"){
            continue;
        }
        if (!next || path.string() < next->string()){
            next = path;
        }
    }
    return next;
}


LabRequest parse_lab_request(const std::string& text){
    LabRequest request;
    std::istringstream lines(text);
    std::string line;
    size_t number = 0;
    while (std::getline(lines, line)){
        number++;
        if (!line.empty() && line.back() == '\r'){
            line.pop_back();
        }
        std::vector<std::string> words = split_command_line(line);
        if (words.empty()){
            continue;
        }
        //  The owner line counts only at the top, so a request can't claim
        //  halfway through.
        if (request.lines.empty() && request.owner.empty() && words.size() == 2 && words[0] == "owner"){
            request.owner = words[1];
            continue;
        }
        request.lines.push_back({number, line, std::move(words)});
    }
    return request;
}


std::string journal_ok(size_t number, const std::string& command, Milliseconds start, Milliseconds end){
    return std::format("ok {} {} @{}-{}ms\n", number, command, start.count(), end.count());
}
std::string journal_error(size_t number, const std::string& command, const std::string& reason){
    return std::format("error {} ({}): {}\n", number, command, reason);
}


std::string lab_state_text(const LabState& state){
    return
        "seq=" + state.seq + "\n" +
        "state=" + state.state + "\n" +
        "program=" + state.program + "\n" +
        "owner=" + state.owner + "\n" +
        "error=" + state.error + "\n";
}


std::string LabOwner::claim(const std::string& request_owner, const std::string& token){
    if (token.empty()){
        return "owner claim: missing token";
    }
    if (!m_token.empty() && m_token != token && m_token != request_owner){
        return "owned by " + m_token;
    }
    m_token = token;
    return "";
}
std::string LabOwner::release(const std::string& request_owner){
    if (m_token.empty()){
        return "";
    }
    if (m_token != request_owner){
        return "owned by " + m_token;
    }
    m_token.clear();
    return "";
}
std::string LabOwner::check(const std::string& request_owner) const{
    if (m_token.empty() || m_token == request_owner){
        return "";
    }
    return "owned by " + m_token;
}


std::string KeepAwakeSchedule::set(const std::vector<std::string>& words, WallClock now){
    if (words.size() < 2 || (words[1] != "on" && words[1] != "off")){
        return "keepawake: expected \"on\" or \"off\"";
    }
    Seconds interval = DEFAULT_INTERVAL;
    if (words.size() > 2){
        const char* begin = words[2].c_str();
        char* end = nullptr;
        errno = 0;
        const long seconds = std::strtol(begin, &end, 10);
        if (end == begin || *end != '\0' || errno == ERANGE || seconds < 1){
            return "keepawake: seconds must be a whole number of at least 1, got " + words[2];
        }
        interval = Seconds(seconds);
    }
    m_on = words[1] == "on";
    m_interval = interval;
    m_last_press = now;     //  The first press comes one interval after the command.
    return "";
}

bool KeepAwakeSchedule::due(WallClock now, bool program_running){
    if (!m_on || program_running){
        //  A running program drives the console itself; the schedule restarts
        //  when it stops.
        m_last_press = now;
        return false;
    }
    if (now - m_last_press < m_interval){
        return false;
    }
    m_last_press = now;
    return true;
}



}
