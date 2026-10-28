/*  Lab Console
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A program for developing other programs: it runs command files dropped into
 *  <profile>/LabConsole/inbox, one line at a time, and writes a timed log with
 *  screenshots to <profile>/LabConsole/outbox. Each line either succeeds or stops
 *  the file with an error and a screenshot, so a probe ("press X; expect menu 5000;
 *  shot menu") answers in seconds instead of a rebuild and a full run.
 *
 *  Games add their detectors and commands through the registry below.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_LabConsole_H
#define PokemonAutomation_NintendoSwitch_LabConsole_H

#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include "CommonTools/VisualDetector.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  A named screen detector the console can query ("detect", "expect", "state").
using LabConsoleDetectorFactory = std::function<std::unique_ptr<StaticScreenDetector>(ConsoleHandle& console)>;
void register_lab_console_detector(const std::string& name, LabConsoleDetectorFactory factory);

//  A named command. `words` is the command line split on spaces, words[0] being the name.
//  Throw (ParseException, OperationFailedException, ...) to fail the line.
using LabConsoleCommand = std::function<void(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::vector<std::string>& words, std::ostream& out
)>;
void register_lab_console_command(const std::string& name, const std::string& usage, LabConsoleCommand command);



class LabConsole_Descriptor : public SingleSwitchProgramDescriptor{
public:
    LabConsole_Descriptor();
};

class LabConsole : public SingleSwitchProgramInstance{
public:
    using Descriptor = LabConsole_Descriptor;
    LabConsole();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

    //  A probe has to start from whatever is on screen, including a mostly black frame
    //  that the border check takes for a black border.
    virtual void start_program_border_check(VideoStream& stream, FeedbackType feedback_type) override{}
};



}
}
#endif
