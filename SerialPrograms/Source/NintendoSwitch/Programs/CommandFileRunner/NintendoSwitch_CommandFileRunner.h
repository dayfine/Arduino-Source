/*  Command File Runner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A developer tool that drives the console from plain-text command files, so
 *  a question like "does this detector fire on that screen?" can be answered
 *  in seconds on live video instead of with a rebuild and a full program run.
 *
 *  Drop <name>.txt into <folder>/inbox. Files run one at a time in name order,
 *  one command per line ('#' starts a comment), and are moved to <folder>/done.
 *  Each file's results go to <folder>/outbox/<name>.out, which appears only
 *  once the file has finished (<name>.tmp shows progress until then):
 *
 *      ok <line> <command> @<start>-<end>ms    a command that succeeded
 *      error line <line> (<command>): <why>    the first failure; the file stops here
 *      end                                     always the last line
 *
 *  plus whatever the commands print, such as detector results and the paths
 *  of saved screenshots. A failure also saves a screenshot of the moment.
 *
 *  Example:
 *      press HOME
 *      expect home 3000
 *      shot home_menu
 *
 *  README.md in this folder documents every command. Games add their own
 *  commands and detectors with register_command_file_command() and
 *  register_command_file_detector().
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_CommandFileRunner_H
#define PokemonAutomation_NintendoSwitch_CommandFileRunner_H

#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonTools/VisualDetector.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  What a command gets to work with.
struct CommandFileSession{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    std::ostream& out;      //  Text printed here goes into the .out file.
    std::string outbox;     //  Folder for files a command saves, ending in '/'.
};

//  A command. `words` is its line split on spaces; words[0] is the command name.
//  Throw (e.g. ParseException, OperationFailedException) to fail the line.
using CommandFileCommand = std::function<void(CommandFileSession& session, const std::vector<std::string>& words)>;

//  A screen detector, by name, for "detect", "wait_for", "expect" and "state".
using CommandFileDetectorFactory = std::function<std::unique_ptr<StaticScreenDetector>(ConsoleHandle& console)>;

//  `usage` is the line "help" prints, e.g. "walk DIR steps".
void register_command_file_command(const std::string& name, const std::string& usage, CommandFileCommand command);
void register_command_file_detector(const std::string& name, CommandFileDetectorFactory factory);



class CommandFileRunner_Descriptor : public SingleSwitchProgramDescriptor{
public:
    CommandFileRunner_Descriptor();
};

class CommandFileRunner : public SingleSwitchProgramInstance{
public:
    using Descriptor = CommandFileRunner_Descriptor;
    CommandFileRunner();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

    //  Command files start from whatever is on screen, including a mostly black
    //  frame that the border check would take for a black border.
    virtual void start_program_border_check(VideoStream& stream, FeedbackType feedback_type) override{}

private:
    std::string folder() const;

    StaticTextOption DESCRIPTION;
    StringOption FOLDER;
    BooleanCheckBoxOption STOP_WHEN_INBOX_EMPTY;
};



}
}
#endif
