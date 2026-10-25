/*  BBQ Lab Console
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Lab only. Runs command files dropped into ~/BBQLabConsole/inbox, one at a time,
 *  so an agent can drive the Switch without keyboard focus. Each file's results go to
 *  ~/BBQLabConsole/outbox/<name>.out, with screenshots next to it.
 *
 */

#ifndef PokemonAutomation_PokemonSV_BBQLabConsole_H
#define PokemonAutomation_PokemonSV_BBQLabConsole_H

#include <string>
#include <vector>
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_BBQOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


//  Split a command line into words. A word starting with '#' starts a comment that runs
//  to the end of the line, so comment lines and blank lines give no words.
std::vector<std::string> bbq_console_words(const std::string& line);

//  Buttons named like "B" or "L+PLUS". Throws ParseException for an unknown name.
Button bbq_console_buttons(const std::string& text);


class BBQLabConsole_Descriptor : public SingleSwitchProgramDescriptor{
public:
    BBQLabConsole_Descriptor();
};

class BBQLabConsole : public SingleSwitchProgramInstance{
public:
    using Descriptor = BBQLabConsole_Descriptor;
    BBQLabConsole();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    BBQOption BBQ_OPTIONS;
};

}
}
}
#endif
