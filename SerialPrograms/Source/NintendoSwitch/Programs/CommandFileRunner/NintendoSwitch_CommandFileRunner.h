/*  Command File Runner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console commands of the lab channel (CommonFramework/Windows/Lab.md):
 *  press buttons, take screenshots, check detectors on the live console, so
 *  a question like "does this detector fire on that screen?" is answered in
 *  seconds instead of with a rebuild and a full program run.
 *
 *  The channel runs these for the selected program while it is stopped, from
 *  requests in "<user files>/Lab/inbox". This file also holds the Command
 *  File Runner, a program that runs the same command files from a folder of
 *  its own while it runs, for batches that should keep going when the lab
 *  channel is off.
 *
 *  A command file has one command per line ('#' starts a comment). Its
 *  journal has one line per command:
 *
 *      ok <line> <command> @<start>-<end>ms    a command that succeeded
 *      error <line> (<command>): <why>         the first failure; the file stops here
 *      end                                     always the last line
 *
 *  plus whatever the commands print, such as detector results and the paths
 *  of saved screenshots. A failure also saves a screenshot of the moment.
 *
 *  Games add their own commands and detectors with
 *  register_command_file_command() and register_command_file_detector().
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
    ConsoleHandle& console;
    ProControllerContext& context;
    std::ostream& out;      //  Text printed here goes into the journal.
    std::string outbox;     //  Folder for files a command saves, ending in '/'.
    std::string name;       //  The command file's name: saved files start with it.

    //  Whether a frame is taken only after the controller's queue has run
    //  dry, so a screenshot shows the state after the presses before it.
    //  False for a command that only reads the screen while a program drives
    //  the controller: it must not wait for the program's presses.
    bool sync_controller = true;
};

//  A command. `words` is its line split on spaces; words[0] is the command name.
//  Throw (e.g. ParseException, OperationFailedException) to fail the line.
using CommandFileCommand = std::function<void(CommandFileSession& session, const std::vector<std::string>& words)>;

//  A screen detector, by name, for "detect", "wait_for", "expect" and "state".
using CommandFileDetectorFactory = std::function<std::unique_ptr<StaticScreenDetector>(ConsoleHandle& console)>;

//  `usage` is the line "help" prints, e.g. "walk DIR steps". A registered
//  command counts as driving the console: the lab channel runs it only while
//  no program runs.
void register_command_file_command(const std::string& name, const std::string& usage, CommandFileCommand command);
void register_command_file_detector(const std::string& name, CommandFileDetectorFactory factory);


enum class CommandFileCommandKind{
    UNKNOWN,
    READS_STREAM,       //  Screenshots and detectors.
    DRIVES_CONSOLE,     //  Presses and waits.
};
CommandFileCommandKind command_file_command_kind(const std::string& name);

//  Runs one command line. Returns false for "stop". Throws to fail the line.
bool run_command_file_line(CommandFileSession& session, const std::vector<std::string>& words);

//  Saves the current frame as <outbox>/<name>_<label>.png and returns its
//  path, or "(no video frame)".
std::string save_command_file_screenshot(CommandFileSession& session, const std::string& label);

//  The lines "help" prints: every command and detector.
void print_command_file_help(std::ostream& out);



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
