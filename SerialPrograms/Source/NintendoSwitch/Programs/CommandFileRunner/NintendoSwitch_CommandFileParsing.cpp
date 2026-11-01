/*  Command File Parsing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cerrno>
#include <cstdlib>
#include <format>
#include <map>
#include <sstream>
#include "Common/Cpp/Exceptions.h"
#include "NintendoSwitch_CommandFileParsing.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


const std::string& required_arg(const std::vector<std::string>& words, size_t index){
    if (index >= words.size()){
        throw ParseException(std::format("{}: missing argument {}", words[0], index));
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
        throw ParseException(std::format("{}: argument {} is not a number: {}", words[0], index, words[index]));
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

TapsArguments parse_taps(const std::vector<std::string>& words){
    //  160 ms is a whole step in BDSP; 640 ms lets the step animation finish
    //  before the next frame is compared. A step there measures 21 to 50, a
    //  bump into a wall or an NPC brushing past 4 to 13.
    constexpr Milliseconds TAP_HOLD(160);
    constexpr Milliseconds TAP_RELEASE(640);
    constexpr double TAP_STILL_RMSD = 16;

    TapsArguments taps;
    taps.direction = parse_dpad(required_arg(words, 1));
    const double count = optional_number(words, 2, 0);
    if (words.size() < 3 || count < 1 || count != (size_t)count){
        throw ParseException(words[0] + ": argument 2 must be a whole number of taps, got " + (words.size() < 3 ? "nothing" : words[2]));
    }
    taps.count = (size_t)count;
    size_t next = 3;
    if (words.size() > next && words[next] == "all"){
        taps.all = true;
        next++;
    }
    taps.hold = optional_duration(words, next, TAP_HOLD);
    taps.release = optional_duration(words, next + 1, TAP_RELEASE);
    taps.still = optional_number(words, next + 2, TAP_STILL_RMSD);
    return taps;
}



}
}
