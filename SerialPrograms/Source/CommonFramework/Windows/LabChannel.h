/*  Lab Channel
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Lets scripts drive a running SerialPrograms app through files, so a lab
 *  can start and stop the selected program, and press buttons or check
 *  detectors on the console, without clicking the window.
 *  Only active when the folder "<user files>/Lab/" exists, so normal users
 *  see no change. Lab.md in this folder documents the commands and files.
 *
 *  A request is a file "Lab/inbox/<name>.txt", one command per line, run in
 *  order from a worker thread and then moved to "Lab/done/". Its journal
 *  "Lab/outbox/<name>.out" has one line per command and ends with "end".
 *  "Lab/state" is rewritten after every request and whenever the program
 *  state changes.
 *
 *  The channel owns the worker thread and the files. Anything that touches
 *  the window (select, start, stop, set) runs on the UI thread through the
 *  LabChannelHost; console commands run on the worker through the
 *  LabConsoleCommands of the console type (NintendoSwitch_LabCommands.h).
 *
 */

#ifndef PokemonAutomation_LabChannel_H
#define PokemonAutomation_LabChannel_H

#include <atomic>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Concurrency/ConditionVariable.h"
#include "Common/Cpp/Concurrency/Thread.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/Panels/PanelSession.h"
#include "LabRequest.h"

namespace PokemonAutomation{


std::string program_state_name(ProgramState state);


//  The window, as the channel sees it. Called on the UI thread only.
class LabChannelHost{
public:
    virtual ~LabChannelHost() = default;

    //  The panel the window shows, or nullptr.
    virtual PanelSession* lab_current_panel() = 0;

    //  Shows the program with this identifier ("<game>:<program>"), as
    //  clicking it in the program list does. Returns an error message, or "".
    virtual std::string lab_select_program(const std::string& identifier) = 0;
};


//  Where a console command writes: its text goes to the journal, its files
//  to the outbox, named "<request>_<label>".
struct LabCommandOutput{
    std::ostream& out;
    std::string outbox;     //  Ends in '/'.
    std::string request;
    size_t line = 0;        //  The command's line in the request.
};

//  The console commands of one console type: presses, screenshots,
//  detectors. Called on the worker thread.
class LabConsoleCommands{
public:
    enum class Kind{
        UNKNOWN,
        READS_STREAM,       //  Screenshots and detectors: allowed while a program runs.
        DRIVES_CONSOLE,     //  Presses: only while no program runs.
    };

    virtual ~LabConsoleCommands() = default;
    virtual Kind kind(const std::string& command) const = 0;

    //  Runs one command line for the console of `panel`, a stopped program.
    //  Throws (e.g. ParseException, OperationFailedException) to fail the line.
    virtual void run(
        PanelSession& panel, CancellableScope& scope,
        const std::vector<std::string>& words, LabCommandOutput& output
    ) = 0;

    //  The lines "help" prints for these commands.
    virtual void help(std::ostream& out) = 0;
};


class LabChannel{
public:
    LabChannel(LabChannelHost& host, std::unique_ptr<LabConsoleCommands> console);
    ~LabChannel();

    //  The panel the window shows now, or nullptr. Call from the UI thread
    //  whenever it changes, before the old panel is destroyed: a console
    //  command running on the old panel is cancelled and finished first.
    void set_panel(PanelSession* panel);


private:
    static const std::string& folder();

    void run();
    void poll();
    void handle_request(const Filesystem::Path& file);
    std::string run_line(const LabRequestLine& line, LabCommandOutput& output);
    std::string run_console_command(const std::vector<std::string>& words, LabCommandOutput& output);
    std::string run_on_console(const std::function<void(PanelSession& panel, CancellableScope& scope)>& body);
    std::string run_ui_command(const std::vector<std::string>& words, std::ostream& out);
    std::string run_on_ui(const std::function<std::string()>& body);
    void write_state();

    //  The state of the selected program. Call under m_panel_lock.
    void read_program_state(std::string& state, std::string& program) const;


private:
    LabChannelHost& m_host;
    std::unique_ptr<LabConsoleCommands> m_console;

    //  The selected panel, read on the worker under this lock. The UI thread
    //  changes it with set_panel(), which first cancels m_active_scope.
    Mutex m_panel_lock;
    PanelSession* m_panel = nullptr;
    Mutex m_scope_lock;
    CancellableScope* m_active_scope = nullptr;
    bool m_panel_changing = false;

    //  Owned by the worker thread.
    LabState m_state;
    std::string m_written_state;
    bool m_folder_ready = false;

    Mutex m_idle_lock;
    ConditionVariable m_idle_cv;
    std::atomic<bool> m_stopping{false};
    Thread m_thread;
};



}
#endif
