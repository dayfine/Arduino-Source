/*  Command File Parsing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <map>
#include <sstream>
#include "Common/Cpp/Exceptions.h"
#include "NintendoSwitch_CommandFileParsing.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


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
