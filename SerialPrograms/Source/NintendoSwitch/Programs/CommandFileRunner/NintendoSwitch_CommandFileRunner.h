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
 *  Every command also adds a line to "<name>.steps.jsonl" next to the
 *  journal (NintendoSwitch_CommandFileRecorder.h): what ran, when, how it
 *  ended and what the games' detectors and readings saw before and after,
 *  so a story played by hand can be turned into story steps.
 *
 *  Games add their own commands, detectors and readings with
 *  register_command_file_command(), register_command_file_detector() and
 *  register_command_file_reading() (NintendoSwitch_CommandFileReading.h),
 *  from their panel list's constructor
 *  (e.g. PokemonLZA_Panels.cpp), so this module includes no game headers.
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
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "CommonTools/VisualDetector.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  What a command gets to work with.
struct CommandFileSession{
    //  The running program's, or one for the stopped program whose console
    //  the lab channel drives.
    ProgramEnvironment& env;
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

//  What a command does with the console, which decides when the lab channel
//  lets it run.
enum class CommandFileCommandKind{
    UNKNOWN,
    READS_STREAM,       //  Screenshots and detectors: also while a program runs.
    DRIVES_CONSOLE,     //  Presses and waits: only while no program runs.
};

//  Adds a command. `usage` is the line "help" prints, e.g. "walk DIR steps".
//  The command parses its own words, throwing ParseException for bad ones.
void register_command_file_command(
    std::string name, std::string usage, CommandFileCommandKind kind, CommandFileCommand command
);

//  Adds a detector. A game's detectors are also recorded in every step.
void register_command_file_detector(std::string name, CommandFileDetectorFactory factory);

//  The kind of a built-in or registered command; UNKNOWN for any other name.
CommandFileCommandKind command_file_command_kind(const std::string& name);

//  Runs line `line` of a command file and adds its step to the steps file.
//  Returns false for "stop". Throws to fail the line, after its step is
//  recorded.
bool run_command_file_line(CommandFileSession& session, const std::vector<std::string>& words, size_t line);

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
