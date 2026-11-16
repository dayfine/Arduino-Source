/*  Lab Channel
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <exception>
#include <sstream>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif
#include <QApplication>
#include <QImage>
#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/ScopeExit.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/ProgramSession.h"
#include "CommonFramework/ImageTypes/ImageRGB32_Qt.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "CommonFramework/VideoPipeline/Backends/VideoFrameQt.h"
#include "LabChannel.h"
#include "LabProgramContract.h"
#include "LabSnapshot.h"
#include "LabTransition.h"

namespace PokemonAutomation{

using namespace std::chrono_literals;


namespace{

//  How often the inbox and the program state are checked.
constexpr Milliseconds POLL_INTERVAL = 500ms;

//  How often the second worker checks the inbox for snapshot requests.
constexpr Milliseconds SNAPSHOT_POLL_INTERVAL = 100ms;

//  How often a worker waiting on the UI thread checks whether the app is closing.
constexpr Milliseconds UI_WAIT_SLICE = 100ms;

//  How often "snapshot" checks whether the app is closing while it waits for a frame.
constexpr Milliseconds SNAPSHOT_WAIT_SLICE = 100ms;

//  Lab/state.json calls the video live while its newest frame is at most this old.
constexpr Milliseconds VIDEO_LIVE_MAX_AGE = 2000ms;


uint64_t process_id(){
#if defined(_WIN32)
    return (uint64_t)_getpid();
#else
    return (uint64_t)getpid();
#endif
}

std::optional<std::string> startup_profile(){
#if defined(__APPLE__)
    if (!Filesystem::STARTUP_PROFILE().empty()){
        return Filesystem::STARTUP_PROFILE();
    }
#endif
    return std::nullopt;
}


//  The channel's own commands, for "help".
void print_channel_help(std::ostream& out){
    out <<
        "channel:\n"
        "  owner claim TOKEN | owner release   (then start with a line \"owner TOKEN\")\n"
        "  select GAME:PROGRAM\n"
        "  set OPTION_KEY JSON\n"
        "  start [SECONDS] | stop   (start waits up to SECONDS, default 30, for the controller)\n"
        "  state\n"
        "  snapshot LABEL [TIMEOUT_MS]   (saves the first video frame newer than the call; default 5000 ms)\n"
        "  describe_program   (the selected program's id and option contract, as one line of JSON)\n"
        "  preflight JSON   (runs the program's start checks with these options, without starting it;\n"
        "                    puts the options back; one line \"preflight JSON\")\n"
        "  keepawake on|off [seconds]\n"
        "  popups [close]   (the app's message boxes; close presses their default button)\n  help\n";
}

//  Saves `frame` as a PNG at `path`. Returns false if it cannot.
bool save_video_frame(const VideoFrame& frame, const std::string& path){
    if (!frame.is_valid()){
        return false;
    }
    //  As SnapshotManager converts the frames programs see.
    QImage qimage = frame.frame.toImage();
    if (qimage.format() != QImage::Format_ARGB32 && qimage.format() != QImage::Format_RGB32){
        qimage = qimage.convertToFormat(QImage::Format_ARGB32);
    }
    const ImageRGB32 image = QImage_to_ImageRGB32(std::move(qimage));
    return image && image.save(path);
}


void write_file(const Filesystem::Path& path, const std::string& text){
    FileIO file;
    if (file.open(path, FileMode::WRITE | FileMode::BINARY)){
        file.write(text);
    }
}

//  Writes `text` to `path` in one step, so a reader never sees a partial file.
void write_file_atomically(const Filesystem::Path& path, const std::string& text){
    const Filesystem::Path temp = Filesystem::unique_temp_path(path);
    write_file(temp, text);
    std::error_code ec;
    Filesystem::rename(temp, path, ec);
}

//  Returns the error message of the start or stop, or empty on success.
//  Unlike the Start/Stop button, a refused start (e.g. an option that fails
//  check_validity()) goes to the journal, so the request stops there.
//  `request_id` names the lab request in the program's last transition.
std::string start_or_stop(PanelSession* panel, bool start, const std::string& request_id){
    ProgramSession* program = dynamic_cast<ProgramSession*>(panel);
    if (program == nullptr){
        return "The selected panel is not a program.";
    }
    const ProgramState state = program->current_state();
    const ProgramState from = start ? ProgramState::STOPPED : ProgramState::RUNNING;
    if (state != from){
        return "Program is " + program_state_name(state) + ".";
    }
    return start
        ? program->start_program(ActionSource::LAB, request_id)
        : program->stop_program(ActionSource::LAB, request_id);
}

//  The value at a dotted key path ("NOTIFICATIONS.Enabled") in a JSON
//  object, or nullptr.
JsonValue* find_key_path(JsonValue& json, const std::string& path){
    JsonValue* value = &json;
    size_t start = 0;
    while (true){
        JsonObject* object = value->to_object();
        if (object == nullptr){
            return nullptr;
        }
        const size_t dot = path.find('.', start);
        value = object->get_value(path.substr(start, dot == std::string::npos ? dot : dot - start));
        if (value == nullptr || dot == std::string::npos){
            return value;
        }
        start = dot + 1;
    }
}

std::string option_keys(const JsonValue& json){
    std::string keys;
    const JsonObject* object = json.to_object();
    if (object == nullptr){
        return keys;
    }
    for (const auto& item : *object){
        keys += (keys.empty() ? "" : " ") + item.first;
    }
    return keys;
}

//  "set KEY JSON": sets one option of the panel, as editing its entry in the
//  settings file would, but live. `text` is the JSON as written on the line.
//  Returns an error message, or "". `result` is the value the panel then
//  holds, so a script can see whether the panel took it (an out-of-range
//  number is clamped, a bad enum ignored).
std::string set_option(PanelSession* panel, const std::string& key, const std::string& text, std::string& result){
    if (panel == nullptr){
        return "No panel is selected.";
    }
    if (key.empty() || text.empty()){
        return "set: expected set KEY JSON";
    }
    const ProgramSession* program = dynamic_cast<const ProgramSession*>(panel);
    if (program != nullptr && program->current_state() != ProgramState::STOPPED){
        return "Program is " + program_state_name(program->current_state()) + "; stop it before changing options.";
    }
    JsonValue value = parse_json(text);
    if (value.is_null() && text != "null"){
        return "set: not JSON: " + text;
    }

    JsonValue json = panel->to_json();
    JsonValue* slot = find_key_path(json, key);
    if (slot == nullptr){
        return "set: no option \"" + key + "\"; options: " + option_keys(json);
    }
    *slot = std::move(value);

    //  A console program's "ConsoleSetup" (camera, controller, audio) reloads
    //  the devices when loaded, so leave it out unless that is what is being set.
    if (key.rfind("ConsoleSetup", 0) != 0){
        JsonObject without;
        for (auto& item : *json.to_object()){
            if (item.first != "ConsoleSetup"){
                without[item.first] = std::move(item.second);
            }
        }
        json = std::move(without);
    }
    panel->load_json(json);
    panel->save_settings();

    JsonValue after = panel->to_json();
    const JsonValue* now = find_key_path(after, key);
    result = now == nullptr ? "" : now->dump(-1);
    return "";
}

}



std::string program_state_name(ProgramState state){
    switch (state){
    case ProgramState::NOT_READY:
        return "NOT_READY";
    case ProgramState::STOPPED:
        return "STOPPED";
    case ProgramState::RUNNING:
        return "RUNNING";
    case ProgramState::STOPPING:
        return "STOPPING";
    }
    return "UNKNOWN";
}

std::string console_command_refusal(ProgramState state, bool reads_only){
    if (reads_only || state == ProgramState::STOPPED){
        return "";
    }
    return "Program is " + program_state_name(state) + "; only commands that read the screen run now.";
}



std::unique_ptr<LabChannel> LabChannel::start_if_enabled(
    LabChannelCallbacks callbacks, std::unique_ptr<LabConsoleCommands> console
){
    if (!Filesystem::is_directory(Filesystem::Path(folder()))){
        return nullptr;
    }
    return std::unique_ptr<LabChannel>(new LabChannel(std::move(callbacks), std::move(console)));
}
LabChannel::LabChannel(LabChannelCallbacks callbacks, std::unique_ptr<LabConsoleCommands> console)
    : m_callbacks(std::move(callbacks))
    , m_console(std::move(console))
    , m_instance_id(new_lab_uuid())
{
    //  A pool task rather than a Thread: Thread::join() does not wait under
    //  the Qt thread-adoption workaround, and the worker must be gone before
    //  the members it uses are.
    m_worker = GlobalThreadPools::unlimited_normal().dispatch([this]{ run(); });
    m_snapshot_worker = GlobalThreadPools::unlimited_normal().dispatch([this]{ run_snapshots(); });
}
LabChannel::~LabChannel(){
    m_stopping.store(true, std::memory_order_release);
    {
        std::lock_guard<Mutex> lg(m_idle_lock);
        m_idle_cv.notify_all();
    }
    set_panel(nullptr);
    m_worker.wait_and_ignore_exceptions();
    m_snapshot_worker.wait_and_ignore_exceptions();
}

const std::string& LabChannel::folder(){
    static const std::string FOLDER = USER_FILE_PATH() + "Lab/";
    return FOLDER;
}

void LabChannel::set_panel(PanelSession* panel){
    {
        std::lock_guard<Mutex> lg(m_scope_lock);
        m_panel_changing = true;
        if (m_active_scope != nullptr){
            m_active_scope->cancel(nullptr);
        }
    }
    std::lock_guard<Mutex> lg(m_panel_lock);
    attach_listeners(panel);
    m_panel = panel;
    m_selected_at = current_time();
    std::lock_guard<Mutex> lg2(m_scope_lock);
    m_panel_changing = false;
}

void LabChannel::attach_listeners(PanelSession* panel){
    if (m_video != nullptr){
        m_video->remove_frame_listener(m_frame_listener);
    }
    m_program = dynamic_cast<ProgramSession*>(panel);
    m_video = panel == nullptr ? nullptr : m_console->video(*panel);
    if (m_video != nullptr){
        m_video->add_frame_listener(m_frame_listener);
    }
}

void LabChannel::FrameListener::on_frame(std::shared_ptr<const VideoFrame> frame){
    const WallClock captured_at = frame->timestamp;
    m_frames.on_frame(captured_at, std::move(frame));
}


void LabChannel::run(){
    while (true){
        {
            std::unique_lock<Mutex> lg(m_idle_lock);
            if (m_stopping.load(std::memory_order_acquire)){
                return;
            }
            m_idle_cv.wait_for(lg, POLL_INTERVAL);
        }
        if (m_stopping.load(std::memory_order_acquire)){
            return;
        }
        try{
            poll();
        }catch (Exception& e){
            global_logger_tagged().log("Lab: " + e.to_str(), COLOR_RED);
        }catch (std::exception& e){
            global_logger_tagged().log(std::string("Lab: ") + e.what(), COLOR_RED);
        }
    }
}

void LabChannel::poll(){
    const Filesystem::Path root(folder());
    if (!Filesystem::is_directory(root)){
        m_folder_ready = false;
        m_written_state.clear();
        return;
    }
    if (!m_folder_ready){
        for (const char* sub : {"inbox", "outbox", "done"}){
            Filesystem::create_directories(root / sub);
        }
        m_folder_ready = true;
        global_logger_tagged().log("Lab: reading requests from " + folder() + "inbox");
    }

    //  Oldest first; after each request the inbox is read again, so one
    //  written meanwhile is not skipped.
    bool ran = true;
    while (ran && !m_stopping.load(std::memory_order_acquire)){
        ran = false;
        for (const Filesystem::Path& file : request_files(folder() + "inbox")){
            if (m_stopping.load(std::memory_order_acquire)){
                break;
            }
            if (take_request(file, false)){
                ran = true;
                break;
            }
        }
    }
    keep_awake();
    write_state();
}

void LabChannel::run_snapshots(){
    while (true){
        {
            std::unique_lock<Mutex> lg(m_idle_lock);
            if (m_stopping.load(std::memory_order_acquire)){
                return;
            }
            m_idle_cv.wait_for(lg, SNAPSHOT_POLL_INTERVAL);
        }
        if (m_stopping.load(std::memory_order_acquire)){
            return;
        }
        try{
            poll_snapshots();
        }catch (Exception& e){
            global_logger_tagged().log("Lab: snapshot: " + e.to_str(), COLOR_RED);
        }catch (std::exception& e){
            global_logger_tagged().log(std::string("Lab: snapshot: ") + e.what(), COLOR_RED);
        }
    }
}

void LabChannel::poll_snapshots(){
    const std::string inbox = folder() + "inbox";
    if (!Filesystem::is_directory(Filesystem::Path(inbox))){
        return;
    }
    for (const Filesystem::Path& file : request_files(inbox)){
        if (m_stopping.load(std::memory_order_acquire)){
            return;
        }
        take_request(file, true);
    }
}

bool LabChannel::take_request(const Filesystem::Path& file, bool snapshots){
    std::string text;
    if (!file_to_string(file.string(), text)){
        //  The other worker took it, or the lab server withdrew it.
        return false;
    }
    const LabRequest request = parse_lab_request(text);
    if (is_lab_snapshot_request(request) != snapshots){
        return false;
    }
    //  The move is the claim: only one worker's rename succeeds.
    std::error_code ec;
    Filesystem::rename(file, Filesystem::Path(folder() + "done/" + file.filename().string()), ec);
    if (ec){
        return false;
    }
    run_request(file.stem().string(), request, snapshots);
    return true;
}

void LabChannel::run_request(const std::string& name, const LabRequest& request, bool snapshots){
    global_logger_tagged().log("Lab: running " + name);
    std::ostringstream out;
    LabCommandOutput output{out, folder() + "outbox/", name, 0};
    const Filesystem::Path out_path(output.outbox + name + ".out");
    const Filesystem::Path tmp_path(output.outbox + name + ".tmp");

    const WallClock start = current_time();
    std::string error;
    for (const LabRequestLine& line : request.lines){
        const WallClock line_start = current_time();
        output.line = line.number;
        //  A snapshot request's lines touch nothing the inbox worker owns.
        error = snapshots ? run_snapshot(line.words, output) : run_line(request, line, output);
        if (!error.empty()){
            out << journal_error(line.number, line.text, error);
            break;
        }
        out << journal_ok(
            line.number, line.text,
            std::chrono::duration_cast<Milliseconds>(line_start - start),
            std::chrono::duration_cast<Milliseconds>(current_time() - start)
        );
        write_file(tmp_path, out.str());
    }
    out << "end\n";
    write_file(tmp_path, out.str());
    Filesystem::remove(out_path);
    Filesystem::rename(tmp_path, out_path);

    std::lock_guard<Mutex> lg(m_last_request_lock);
    m_last_request = name;
    m_last_error = error;
}

std::string LabChannel::run_line(const LabRequest& request, const LabRequestLine& line, LabCommandOutput& output){
    const std::vector<std::string>& words = line.words;
    const std::string& command = words[0];

    //  Anyone may look. Everything else needs the owner's token once claimed.
    if (command == "owner"){
        if (words.size() >= 3 && words[1] == "claim"){
            return m_owner.claim(request.owner, words[2]);
        }
        if (words.size() == 2 && words[1] == "release"){
            return m_owner.release(request.owner);
        }
        return "owner: expected owner claim TOKEN, or owner release";
    }
    if (command == "help"){
        print_channel_help(output.out);
        m_console->help(output.out);
        return "";
    }
    if (command == "state"){
        write_state();
        output.out << m_written_state;
        return "";
    }
    if (command == "snapshot"){
        return run_snapshot(words, output);
    }
    if (command == "describe_program"){
        return run_describe_program(words, output.out);
    }
    const LabConsoleCommands::Kind kind = m_console->kind(command);
    if (kind != LabConsoleCommands::Kind::READS_STREAM){
        std::string error = m_owner.check(request.owner);
        if (!error.empty()){
            return error;
        }
    }
    if (command == "select" || command == "set" || command == "start" || command == "stop" || command == "popups"
        || command == "preflight"
    ){
        return run_ui_command(request, line, output.out);
    }
    if (command == "keepawake"){
        return m_keep_awake.set(words, current_time());
    }
    if (kind == LabConsoleCommands::Kind::UNKNOWN){
        return "Unknown command: " + command + " (try \"help\")";
    }
    return run_console_command(words, output);
}

//  "describe_program": the selected program's id and the static contract of
//  its options (LabProgramContract.h), as one line of JSON. Reads only.
std::string LabChannel::run_describe_program(const std::vector<std::string>& words, std::ostream& out){
    if (words.size() != 1){
        return "describe_program: expected describe_program";
    }
    std::lock_guard<Mutex> lg(m_panel_lock);
    if (m_panel == nullptr || m_program == nullptr){
        return "describe_program: the selected panel is not a program.";
    }
    const BatchOption* options = m_console->options(*m_panel);
    if (options == nullptr){
        return "describe_program: the selected program has no options the lab can read.";
    }
    JsonValue contract = lab_program_contract(
        m_program->descriptor().identifier(), *options, m_program->prechecks().summaries()
    );
    out << "describe_program " << contract.dump(-1) << "\n";
    return "";
}

//  "preflight JSON": a checks-only run of the selected program with these
//  options (ProgramSession::Prechecks::run_only()) on this worker, holding
//  the panel like a console command. Prints one line, "preflight " and
//  lab_preflight_reply() as JSON. A program that is not STOPPED, a
//  controller that is not ready or a check that throws fails the line
//  instead; the options are put back all the same.
std::string LabChannel::run_preflight(const LabRequestLine& line, std::ostream& out){
    const JsonValue candidate = parse_json(line_after_words(line.text, 1));
    if (candidate.to_object() == nullptr){
        return "preflight: expected preflight JSON, an object of option keys";
    }
    return run_on_console([&](PanelSession& panel, CancellableScope& scope){
        ProgramSession* program = dynamic_cast<ProgramSession*>(&panel);
        if (program == nullptr){
            throw ParseException("The selected panel is not a program.");
        }
        const PrecheckOnlyRun run = program->prechecks().run_only(candidate, scope);
        JsonValue reply = lab_preflight_reply(program->descriptor().identifier(), run, read_options_hash(panel));
        out << "preflight " << reply.dump(-1) << "\n";
    });
}

std::optional<std::string> LabChannel::read_options_hash(PanelSession& panel){
    const BatchOption* options = m_console->options(panel);
    if (options == nullptr){
        return std::nullopt;
    }
    return lab_options_hash(options->to_json());
}

std::string LabChannel::run_console_command(const std::vector<std::string>& words, LabCommandOutput& output){
    const bool reads_only = m_console->kind(words[0]) == LabConsoleCommands::Kind::READS_STREAM;
    return run_on_console([&](PanelSession& panel, CancellableScope& scope){
        const ProgramSession* program = dynamic_cast<const ProgramSession*>(&panel);
        if (program == nullptr){
            throw ParseException("The selected panel is not a program.");
        }
        const std::string refusal = console_command_refusal(program->current_state(), reads_only);
        if (!refusal.empty()){
            throw ParseException(refusal);
        }
        m_console->run(panel, scope, words, output);
    });
}

//  Runs `body` with the selected panel, holding it against a panel change
//  until the body returns. Returns an error message, or "".
std::string LabChannel::run_on_console(const std::function<void(PanelSession& panel, CancellableScope& scope)>& body){
    std::lock_guard<Mutex> lg(m_panel_lock);
    if (m_panel == nullptr){
        return "No panel is selected.";
    }
    CancellableHolder<CancellableScope> scope;
    {
        std::lock_guard<Mutex> lg2(m_scope_lock);
        m_active_scope = &scope;
        if (m_panel_changing){
            //  set_panel() cancelled before this scope existed and is waiting
            //  for m_panel_lock.
            scope.cancel(nullptr);
        }
    }
    ScopeExit clear_scope([this]{
        std::lock_guard<Mutex> lg2(m_scope_lock);
        m_active_scope = nullptr;
    });
    try{
        body(*m_panel, scope);
    }catch (OperationCancelledException&){
        return "cancelled: the panel changed.";
    }catch (Exception& e){
        return e.message();
    }catch (std::exception& e){
        return e.what();
    }
    return "";
}

std::string LabChannel::run_ui_command(const LabRequest& request, const LabRequestLine& line, std::ostream& out){
    const std::vector<std::string>& words = line.words;
    const std::string& command = words[0];
    if (command == "popups"){
        return run_popups(words, out);
    }
    if (command == "preflight"){
        //  Stages options like "set", but on the program session, from this worker.
        return run_preflight(line, out);
    }
    if (command == "select"){
        if (words.size() < 2){
            return "select: expected select GAME:PROGRAM";
        }
        const std::string& identifier = words[1];
        return run_on_ui([&]{
            std::string error = m_callbacks.select_program(identifier);
            if (!error.empty()){
                return error;
            }
            PanelSession* panel = m_callbacks.current_panel();
            if (panel == nullptr || panel->descriptor().identifier() != identifier){
                return "select: the window did not switch to " + identifier;
            }
            return std::string();
        });
    }
    if (command == "set"){
        //  The JSON is taken from the line as written: words would lose the
        //  spaces and '#' inside a string.
        const std::string key = words.size() > 1 ? words[1] : "";
        const std::string text = line_after_words(line.text, 2);
        std::string result;
        std::string error = run_on_ui([&]{
            return set_option(m_callbacks.current_panel(), key, text, result);
        });
        if (error.empty()){
            out << "set " << key << " = " << result << "\n";
        }
        return error;
    }
    if (command == "start"){
        return run_start(request, words);
    }
    return run_start_or_stop(request, false);
}

//  Starts or stops the program on the UI thread, as the lab with the
//  request's id (a new one if the request has none), so the program's last
//  transition names this request.
std::string LabChannel::run_start_or_stop(const LabRequest& request, bool start){
    const std::string id = request.id ? *request.id : new_lab_uuid();
    return run_on_ui([&]{
        return start_or_stop(m_callbacks.current_panel(), start, id);
    });
}

//  "start [SECONDS]": waits for the controller, then starts. The wait runs
//  here on the worker, so the window stays responsive while it reconnects.
std::string LabChannel::run_start(const LabRequest& request, const std::vector<std::string>& words){
    Seconds timeout = CONTROLLER_WAIT_DEFAULT;
    if (words.size() > 2){
        return "start: expected start [SECONDS]";
    }
    if (words.size() == 2){
        const std::optional<Seconds> seconds = parse_seconds(words[1]);
        if (!seconds){
            return "start: seconds must be a whole number of at least 1, got " + words[1];
        }
        timeout = *seconds;
    }
    const std::string error = wait_for_controller(
        [this]{
            std::lock_guard<Mutex> lg(m_panel_lock);
            return read_controller_status();
        },
        [this](Milliseconds slice){
            std::unique_lock<Mutex> lg(m_idle_lock);
            m_idle_cv.wait_for(lg, slice);
            return !m_stopping.load(std::memory_order_acquire);
        },
        timeout, POLL_INTERVAL
    );
    if (!error.empty()){
        return error;
    }
    return run_start_or_stop(request, true);
}

//  Runs `body` on the UI thread and returns what it returns: "" or an error.
//  Gives up with an error if the app is closing, since the UI thread would
//  never run it.
//  A failed program leaves a modal "Error" box, and nothing at the
//  keyboard closes it on a lab console. "popups" lists the app's open
//  message boxes; "popups close" presses each one's default button.
std::string LabChannel::run_popups(const std::vector<std::string>& words, std::ostream& out){
    const bool close = words.size() >= 2 && words[1] == "close";
    if (words.size() > 2 || (words.size() == 2 && !close)){
        return "popups: expected popups, or popups close";
    }
    return run_on_ui([&]{
        size_t count = 0;
        for (QWidget* widget : QApplication::topLevelWidgets()){
            QMessageBox* box = qobject_cast<QMessageBox*>(widget);
            if (box == nullptr || !box->isVisible()){
                continue;
            }
            count++;
            out << "popup: " << box->windowTitle().toStdString() << ": " << box->text().toStdString() << "\n";
            if (close){
                QPushButton* button = box->defaultButton();
                if (button != nullptr){
                    button->click();
                }else{
                    box->done(0);
                }
            }
        }
        out << count << (close ? " closed" : " open") << "\n";
        return std::string();
    });
}

//  "snapshot LABEL [TIMEOUT_MS]": saves the first frame newer than this line
//  and names it. Reads only, so it needs no owner token; it needs no program
//  state or controller either, only the selected panel's video.
std::string LabChannel::run_snapshot(const std::vector<std::string>& words, LabCommandOutput& output){
    const std::expected<LabSnapshotArgs, std::string> args = parse_lab_snapshot(words);
    if (!args){
        return args.error();
    }
    auto reply = [&](const LabSnapshotReply& r){
        output.out << r.line;
        return r.error;
    };
    VideoFeed* video;
    {
        std::lock_guard<Mutex> lg(m_panel_lock);
        video = m_video;
    }
    if (video == nullptr){
        return reply(lab_snapshot_unavailable(m_instance_id, LabSnapshotUnavailable::NO_VIDEO));
    }

    const LabFrameWait wait = m_frame_listener.frames().wait_for_next_frame(
        current_time() + args->timeout, SNAPSHOT_WAIT_SLICE,
        [this]{ return m_stopping.load(std::memory_order_acquire); }
    );
    if (wait.cancelled){
        return reply(lab_snapshot_unavailable(m_instance_id, LabSnapshotUnavailable::APP_CLOSING));
    }
    if (!wait.frame){
        return reply(lab_snapshot_timeout(m_instance_id, wait.after, args->timeout));
    }
    {
        //  A frame counted after a panel change may be another console's.
        std::lock_guard<Mutex> lg(m_panel_lock);
        if (m_video != video){
            return reply(lab_snapshot_unavailable(m_instance_id, LabSnapshotUnavailable::PANEL_CHANGED));
        }
    }
    const std::string path = output.outbox + output.request + "_" + args->label + ".png";
    if (wait.frame->frame == nullptr || !save_video_frame(*wait.frame->frame, path)){
        return reply(lab_snapshot_unavailable(m_instance_id, LabSnapshotUnavailable::SAVE_FAILED));
    }
    return reply(lab_snapshot_saved(m_instance_id, wait.frame->token, wait.after, path));
}

std::string LabChannel::run_on_ui(const std::function<std::string()>& body){
    struct Call{
        Mutex lock;
        ConditionVariable cv;
        bool done = false;
        bool abandoned = false;
        std::string result;
    };
    auto call = std::make_shared<Call>();
    QMetaObject::invokeMethod(QCoreApplication::instance(), [call, &body]{
        //  Held while the body runs, so the worker can't abandon the call
        //  (and its `body`) halfway through.
        std::lock_guard<Mutex> lg(call->lock);
        if (call->abandoned){
            return;
        }
        try{
            call->result = body();
        }catch (Exception& e){
            call->result = e.message();
        }catch (std::exception& e){
            call->result = e.what();
        }
        call->done = true;
        call->cv.notify_all();
    }, Qt::QueuedConnection);

    std::unique_lock<Mutex> lg(call->lock);
    while (!call->done){
        if (m_stopping.load(std::memory_order_acquire)){
            call->abandoned = true;
            return "The app is closing.";
        }
        call->cv.wait_for(lg, UI_WAIT_SLICE);
    }
    return call->result;
}

void LabChannel::keep_awake(){
    bool running = false;
    {
        std::lock_guard<Mutex> lg(m_panel_lock);
        std::string state, program;
        read_program_state(state, program, running);
    }
    if (!m_keep_awake.due(current_time(), running)){
        return;
    }
    const std::string error = run_on_console([this](PanelSession& panel, CancellableScope& scope){
        m_console->keep_awake(panel, scope);
    });
    if (!error.empty()){
        global_logger_tagged().log("Lab: keepawake: " + error, COLOR_RED);
    }
}

void LabChannel::read_program_state(std::string& state, std::string& program, bool& running) const{
    const ProgramSession* session = dynamic_cast<const ProgramSession*>(m_panel);
    program = m_panel == nullptr ? "" : m_panel->descriptor().identifier();
    if (session == nullptr){
        state = "NO_PROGRAM";
        running = false;
        return;
    }
    const ProgramState current = session->current_state();
    state = program_state_name(current);
    running = current != ProgramState::STOPPED;
}

std::string LabChannel::read_controller_status(){
    return m_panel == nullptr ? "" : m_console->controller_status(*m_panel);
}

void LabChannel::write_state(){
    {
        std::lock_guard<Mutex> lg(m_panel_lock);
        bool running;
        read_program_state(m_state.state, m_state.program, running);
        m_state.controller = read_controller_status();
    }
    {
        std::lock_guard<Mutex> lg(m_last_request_lock);
        m_state.seq = m_last_request;
        m_state.error = m_last_error;
    }
    m_state.owner = m_owner.token();
    std::string text = lab_state_text(m_state);
    const Filesystem::Path path(folder() + "state");
    if (text != m_written_state || !Filesystem::exists(path)){
        write_file_atomically(path, text);
        m_written_state = std::move(text);
    }
    write_state_json(current_time());
}

//  Checked at every poll: written at once on a logical change, else about
//  once a second, the heartbeat ("written_at") and the newest frame.
void LabChannel::write_state_json(WallClock now){
    LabAppState state;
    bool has_video;
    {
        std::lock_guard<Mutex> lg(m_panel_lock);
        has_video = m_video != nullptr;
        if (m_program != nullptr){
            state.last_transition = lab_transition_since(m_program->last_transition(), m_selected_at);
            state.options_hash = read_options_hash(*m_panel);
        }
    }
    state.legacy = m_state;
    state.written_at = now;
    state.instance_id = m_instance_id;
    state.pid = process_id();
    state.profile = startup_profile();
    state.frame = m_frame_listener.frames().latest();
    state.video = lab_video_state(has_video, state.frame, now, VIDEO_LIVE_MAX_AGE);

    const Filesystem::Path path(folder() + "state.json");
    if (!Filesystem::exists(path)){
        m_state_json.reset();
    }
    const std::optional<std::string> text = m_state_json.text_to_write(state);
    if (text){
        write_file_atomically(path, *text);
    }
}



}
