/*  Lab Channel
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Lets scripts drive a running SerialPrograms app through files, so a lab
 *  can select a program, set its options, start and stop it, and press
 *  buttons or check detectors on the console without clicking the window.
 *  Only active when the folder "<user files>/Lab/" exists, so normal users
 *  see no change. Lab.md in this folder documents the commands and files.
 *
 *  A request is a file "Lab/inbox/<name>.txt", one command per line, run in
 *  order from a worker thread and then moved to "Lab/done/". Its journal
 *  "Lab/outbox/<name>.out" has one line per command and ends with "end".
 *  "Lab/state" is rewritten after every request and whenever the program
 *  state changes; "Lab/state.json" (LabStateJson.h) at every poll, for the
 *  lab server.
 *
 *  The channel owns the worker and the files. Anything that touches the
 *  window (select, start, stop, set) runs on the UI thread through the
 *  LabChannelCallbacks the window gives it; console commands run on the
 *  worker through the LabConsoleCommands of the console type
 *  (NintendoSwitch_LabCommands.h).
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
#include "Common/Cpp/Concurrency/AsyncTask.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Concurrency/ConditionVariable.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/Panels/PanelSession.h"
#include "CommonFramework/ProgramSession.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "LabRequest.h"
#include "LabStateJson.h"

namespace PokemonAutomation{


std::string program_state_name(ProgramState state);


//  The window, as the channel sees it. Both are called on the UI thread only.
struct LabChannelCallbacks{
    //  The panel the window shows, or nullptr.
    std::function<PanelSession*()> current_panel;

    //  Shows the program with this identifier ("<game>:<program>"), as
    //  clicking it in the program list does. Returns an error message, or "".
    std::function<std::string(const std::string& identifier)> select_program;
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
        DRIVES_CONSOLE,     //  Presses: only while no program runs, and only for the owner.
    };

    virtual ~LabConsoleCommands() = default;
    virtual Kind kind(const std::string& command) const = 0;

    //  Runs one command line for the console of `panel`, a stopped program.
    //  Throws (e.g. ParseException, OperationFailedException) to fail the line.
    virtual void run(
        PanelSession& panel, CancellableScope& scope,
        const std::vector<std::string>& words, LabCommandOutput& output
    ) = 0;

    //  "ready" when the console's controller can take commands, "none" when
    //  it can't (not connected yet, or reconnecting after a program switch),
    //  "" when `panel` has no console. Read for the state file and by "start".
    virtual std::string controller_status(PanelSession& panel) = 0;

    //  The video of the console of `panel`, or nullptr if it has none.
    //  Called on the UI thread.
    virtual VideoFeed* video(PanelSession& panel) = 0;

    //  The press that keeps the console awake ("keepawake on").
    virtual void keep_awake(PanelSession& panel, CancellableScope& scope) = 0;

    //  The lines "help" prints for these commands.
    virtual void help(std::ostream& out) = 0;
};


class LabChannel{
public:
    //  The channel, if "<user files>/Lab/" exists when the app starts;
    //  nullptr otherwise, and nothing runs. Create the folder before
    //  launching the app.
    static std::unique_ptr<LabChannel> start_if_enabled(
        LabChannelCallbacks callbacks, std::unique_ptr<LabConsoleCommands> console
    );

    //  Stops the worker and waits for it.
    ~LabChannel();

    //  The panel the window shows now, or nullptr. Call from the UI thread
    //  whenever it changes, before the old panel is destroyed: a console
    //  command running on the old panel is cancelled and finished first.
    void set_panel(PanelSession* panel);


private:
    LabChannel(LabChannelCallbacks callbacks, std::unique_ptr<LabConsoleCommands> console);

    static const std::string& folder();

    void run();
    void poll();
    void handle_request(const Filesystem::Path& file);
    std::string run_line(const LabRequest& request, const LabRequestLine& line, LabCommandOutput& output);
    std::string run_console_command(const std::vector<std::string>& words, LabCommandOutput& output);
    std::string run_on_console(const std::function<void(PanelSession& panel, CancellableScope& scope)>& body);
    std::string run_ui_command(const LabRequest& request, const LabRequestLine& line, std::ostream& out);
    std::string run_start_or_stop(const LabRequest& request, bool start);
    std::string run_start(const LabRequest& request, const std::vector<std::string>& words);
    std::string run_popups(const std::vector<std::string>& words, std::ostream& out);
    std::string run_on_ui(const std::function<std::string()>& body);
    void keep_awake();
    void write_state();
    void write_state_json(WallClock now);

    //  Follows the program's starts and stops for LabTransitionTracker.
    class ProgramListener : public ProgramSession::Listener{
    public:
        explicit ProgramListener(LabTransitionTracker& tracker) : m_tracker(tracker) {}
        virtual void state_change(ProgramState state) override;
        virtual void stats_update(const StatsTracker*, const StatsTracker*) override{}
        virtual void error(const std::string& message) override;
        virtual void download_error(const std::string&) override{}
        virtual void download_added(std::shared_ptr<ResourceDownload>) override{}
        virtual void all_downloads_done() override{}
    private:
        LabTransitionTracker& m_tracker;
    };

    //  The newest frame of the selected console's video.
    class FrameListener : public VideoFrameListener{
    public:
        virtual void on_frame(std::shared_ptr<const VideoFrame> frame) override;
        std::optional<LabFrameToken> latest() const;
    private:
        mutable Mutex m_lock;
        uint64_t m_count = 0;   //  Frames since the app started, over every panel.
        std::optional<LabFrameToken> m_latest;
    };

    //  Moves the listeners from the old panel to `panel`. Call under m_panel_lock.
    void attach_listeners(PanelSession* panel);

    //  The state of the selected program. Call under m_panel_lock.
    void read_program_state(std::string& state, std::string& program, bool& running) const;
    //  The selected panel's LabConsoleCommands::controller_status(). Call under m_panel_lock.
    std::string read_controller_status();


private:
    LabChannelCallbacks m_callbacks;
    std::unique_ptr<LabConsoleCommands> m_console;

    //  The selected panel, read on the worker under this lock. The UI thread
    //  changes it with set_panel(), which first cancels m_active_scope.
    Mutex m_panel_lock;
    PanelSession* m_panel = nullptr;
    ProgramSession* m_program = nullptr;    //  m_panel, if a program.
    VideoFeed* m_video = nullptr;           //  m_panel's video, if any.
    Mutex m_scope_lock;
    CancellableScope* m_active_scope = nullptr;
    bool m_panel_changing = false;

    //  For Lab/state.json.
    const std::string m_instance_id;
    LabTransitionTracker m_transitions;
    ProgramListener m_program_listener{m_transitions};
    FrameListener m_frame_listener;

    //  Owned by the worker thread.
    LabOwner m_owner;
    KeepAwakeSchedule m_keep_awake;
    LabState m_state;
    std::string m_written_state;
    bool m_folder_ready = false;

    Mutex m_idle_lock;
    ConditionVariable m_idle_cv;
    std::atomic<bool> m_stopping{false};
    AsyncTask m_worker;     //  Waited for in the destructor, before the members above go.
};



}
#endif
