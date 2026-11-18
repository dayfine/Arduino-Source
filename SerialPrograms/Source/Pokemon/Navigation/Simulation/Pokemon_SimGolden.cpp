/*  Sim Golden
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Pokemon_SimGolden.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  `text` with every "\n" made "\r\n" and no "\r" of its own.
std::string with_crlf(std::string_view text){
    std::string out;
    out.reserve(text.size() + text.size() / 16);
    for (char ch : text){
        if (ch == '\r'){
            continue;
        }
        if (ch == '\n'){
            out += '\r';
        }
        out += ch;
    }
    return out;
}

//  Line `begin` of `text` up to its "\n", without a "\r" at its end.
std::string_view line_at(std::string_view text, size_t begin){
    if (begin >= text.size()){
        return {};
    }
    const size_t end = std::min(text.find('\n', begin), text.size());
    std::string_view line = text.substr(begin, end - begin);
    if (line.ends_with('\r')){
        line.remove_suffix(1);
    }
    return line;
}

//  Where the line after the one at `begin` starts; past the end if none.
size_t next_line(std::string_view text, size_t begin){
    if (begin >= text.size()){
        return text.size() + 1;
    }
    const size_t end = text.find('\n', begin);
    return end == std::string_view::npos ? text.size() + 1 : end + 1;
}

} // namespace


bool sim_golden_update_requested(){
    // code-health: allow char_pointer -- std::getenv returns a const char pointer
    const char* value = std::getenv(std::string(SIM_GOLDEN_UPDATE_VARIABLE).c_str());
    return value != nullptr && value[0] != '\0' && std::string_view(value) != "0";
}

std::string sim_golden_path(std::string_view source_file, std::string_view name){
    const size_t slash = source_file.find_last_of("/\\");
    std::string out = slash == std::string_view::npos ? std::string() : std::string(source_file.substr(0, slash + 1));
    out += name;
    return out;
}

std::string sim_golden_text(std::string_view about, const std::vector<std::string>& lines){
    std::string out = "# ";
    out += about;
    out += "\n";
    for (const std::string& line : lines){
        out += line;
        out += "\n";
    }
    return out;
}

std::string first_golden_difference(std::string_view golden, std::string_view actual){
    size_t a = 0;
    size_t b = 0;
    for (size_t line = 1; a < golden.size() || b < actual.size(); line++){
        const std::string_view golden_line = line_at(golden, a);
        const std::string_view actual_line = line_at(actual, b);
        const bool golden_ended = a >= golden.size();
        const bool actual_ended = b >= actual.size();
        if (golden_line != actual_line || golden_ended != actual_ended){
            return std::format(
                "line {}:\n--- golden: {}\n+++ actual: {}", line,
                golden_ended ? "(end of file)" : golden_line,
                actual_ended ? "(end of file)" : actual_line
            );
        }
        a = next_line(golden, a);
        b = next_line(actual, b);
    }
    return "";
}

UnitTestResult check_sim_golden(Logger& logger, const std::string& path, const std::string& text){
    if (sim_golden_update_requested()){
        const std::string crlf = with_crlf(text);
        FileIO file;
        if (!file.open(path, FileMode::WRITE | FileMode::BINARY) || file.write(crlf) != crlf.size()){
            return "Could not write " + path + ".";
        }
        logger.log("Wrote " + path + ".");
        return true;
    }
    std::string golden;
    if (!file_to_string(path, golden)){
        return std::format("No golden at {}: run with {}=1 to write it.", path, SIM_GOLDEN_UPDATE_VARIABLE);
    }
    const std::string difference = first_golden_difference(golden, text);
    if (!difference.empty()){
        return std::format(
            "{} differs from the simulation at {}\nRun with {}=1 to rewrite it if the change is meant.",
            path, difference, SIM_GOLDEN_UPDATE_VARIABLE
        );
    }
    return true;
}



} // namespace Pokemon
} // namespace PokemonAutomation
