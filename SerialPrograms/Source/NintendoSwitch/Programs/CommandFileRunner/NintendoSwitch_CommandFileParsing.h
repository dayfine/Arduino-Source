/*  Command File Parsing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading command files for the Command File Runner: the words of a line,
 *  button and D-pad names, and which inbox file runs next.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_CommandFileParsing_H
#define PokemonAutomation_NintendoSwitch_CommandFileParsing_H

#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  One line of a command file, split into words. A word starting with '#'
//  starts a comment that runs to the end of the line.
class CommandLine{
public:
    explicit CommandLine(const std::string& line){
        std::istringstream in(line);
        std::string word;
        while (in >> word && word[0] != '#'){
            m_words.push_back(word);
        }
    }

    bool empty() const{ return m_words.empty(); }
    const std::string& name() const{ return m_words[0]; }
    const std::vector<std::string>& words() const{ return m_words; }
    bool has(size_t index) const{ return index < m_words.size(); }

    const std::string& arg(size_t index) const{
        if (!has(index)){
            throw ParseException(name() + ": missing argument " + std::to_string(index));
        }
        return m_words[index];
    }
    double number(size_t index, double fallback) const{
        if (!has(index)){
            return fallback;
        }
        try{
            return std::stod(m_words[index]);
        }catch (std::exception&){
            throw ParseException(name() + ": argument " + std::to_string(index) + " is not a number: " + m_words[index]);
        }
    }
    Milliseconds duration(size_t index, Milliseconds fallback) const{
        return Milliseconds((int64_t)number(index, (double)fallback.count()));
    }

private:
    std::vector<std::string> m_words;
};


//  A button name, or several joined with '+' (e.g. "L+R"). Throws
//  ParseException for an unknown name.
Button parse_buttons(const std::string& text);

//  "UP", "DOWN", "LEFT" or "RIGHT". Throws ParseException otherwise.
DpadPosition parse_dpad(const std::string& text);

//  The ".txt" file in `inbox` whose name sorts first, if any.
std::optional<Filesystem::Path> next_command_file(const std::string& inbox);



}
}
#endif
