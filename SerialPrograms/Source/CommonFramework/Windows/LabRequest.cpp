/*  Lab Request
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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
        request.lines.push_back({number, line, std::move(words)});
    }
    return request;
}


std::string journal_ok(size_t number, const std::string& command, Milliseconds start, Milliseconds end){
    return "ok " + std::to_string(number) + " " + command
        + " @" + std::to_string(start.count()) + "-" + std::to_string(end.count()) + "ms\n";
}
std::string journal_error(size_t number, const std::string& command, const std::string& reason){
    return "error " + std::to_string(number) + " (" + command + "): " + reason + "\n";
}


std::string lab_state_text(const LabState& state){
    return
        "seq=" + state.seq + "\n" +
        "state=" + state.state + "\n" +
        "program=" + state.program + "\n" +
        "error=" + state.error + "\n";
}





}
