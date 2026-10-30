/*  Command File Parsing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cerrno>
#include <cstdlib>
#include <map>
#include <sstream>
#include "Common/Cpp/Exceptions.h"
#include "NintendoSwitch_CommandFileParsing.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


std::vector<std::string> split_command_line(const std::string& line){
    std::vector<std::string> words;
    std::istringstream in(line);
    std::string word;
    while (in >> word && word[0] != '#'){
        words.push_back(word);
    }
    return words;
}

const std::string& required_arg(const std::vector<std::string>& words, size_t index){
    if (index >= words.size()){
        throw ParseException(words[0] + ": missing argument " + std::to_string(index));
    }
    return words[index];
}

double optional_number(const std::vector<std::string>& words, size_t index, double fallback){
    if (index >= words.size()){
        return fallback;
    }
    //  strtod() rather than std::stod(): on the macOS Intel CI build, the
    //  std::invalid_argument that std::stod() throws is not caught as a
    //  std::exception.
    const char* begin = words[index].c_str();
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(begin, &end);
    if (end == begin || errno == ERANGE){
        throw ParseException(words[0] + ": argument " + std::to_string(index) + " is not a number: " + words[index]);
    }
    return value;
}

Milliseconds optional_duration(const std::vector<std::string>& words, size_t index, Milliseconds fallback){
    return Milliseconds((int64_t)optional_number(words, index, (double)fallback.count()));
}


Button parse_buttons(const std::string& text){
    static const std::map<std::string, Button> NAMES{
        {"A", BUTTON_A}, {"B", BUTTON_B}, {"X", BUTTON_X}, {"Y", BUTTON_Y},
        {"L", BUTTON_L}, {"R", BUTTON_R}, {"ZL", BUTTON_ZL}, {"ZR", BUTTON_ZR},
        {"PLUS", BUTTON_PLUS}, {"MINUS", BUTTON_MINUS},
        {"LCLICK", BUTTON_LCLICK}, {"RCLICK", BUTTON_RCLICK},
        {"HOME", BUTTON_HOME}, {"CAPTURE", BUTTON_CAPTURE},
    };
    Button buttons = BUTTON_NONE;
    std::istringstream in(text);
    std::string name;
    while (std::getline(in, name, '+')){
        auto iter = NAMES.find(name);
        if (iter == NAMES.end()){
            throw ParseException("Unknown button: " + name);
        }
        buttons = (Button)(buttons | iter->second);
    }
    return buttons;
}

DpadPosition parse_dpad(const std::string& text){
    static const std::map<std::string, DpadPosition> NAMES{
        {"UP", DPAD_UP}, {"DOWN", DPAD_DOWN}, {"LEFT", DPAD_LEFT}, {"RIGHT", DPAD_RIGHT},
    };
    auto iter = NAMES.find(text);
    if (iter == NAMES.end()){
        throw ParseException("Unknown D-pad direction: " + text);
    }
    return iter->second;
}

std::optional<Filesystem::Path> next_command_file(const std::string& inbox){
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



}
}
