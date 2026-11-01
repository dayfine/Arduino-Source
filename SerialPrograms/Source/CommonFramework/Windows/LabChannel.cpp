/*  Lab Channel
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <exception>
#include <sstream>
#include <QCoreApplication>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/ScopeExit.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/ProgramSession.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "LabChannel.h"

namespace PokemonAutomation{

using namespace std::chrono_literals;


namespace{

//  How often the inbox and the program state are checked.
constexpr Milliseconds POLL_INTERVAL = 500ms;

//  How often a worker waiting on the UI thread checks whether the app is closing.
constexpr Milliseconds UI_WAIT_SLICE = 100ms;


//  Returns the error message of the start or stop, or empty on success.
//  Unlike the Start/Stop button, a refused start (e.g. an option that fails
//  check_validity()) goes to the journal, so the request stops there.
std::string start_or_stop(PanelSession* panel, bool start){
    ProgramSession* program = dynamic_cast<ProgramSession*>(panel);
    if (program == nullptr){
        return "The selected panel is not a program.";
    }
    const ProgramState state = program->current_state();
    const ProgramState from = start ? ProgramState::STOPPED : ProgramState::RUNNING;
    if (state != from){
        return "Program is " + program_state_name(state) + ".";
    }
    return start ? program->start_program() : program->stop_program();
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
    , m_runs(folder() + "runs/")
{
    //  A pool task rather than a Thread: Thread::join() does not wait under
    //  the Qt thread-adoption workaround, and the worker must be gone before
    //  the members it uses are.
    m_worker = GlobalThreadPools::unlimited_normal().dispatch([this]{ run(); });
}
LabChannel::~LabChannel(){
    m_stopping.store(true, std::memory_order_release);
    {
        std::lock_guard<Mutex> lg(m_idle_lock);
        m_idle_cv.notify_all();
    }
    set_panel(nullptr);
    m_worker.wait_and_ignore_exceptions();
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
    m_panel = panel;
    std::lock_guard<Mutex> lg2(m_scope_lock);
    m_panel_changing = false;
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

    std::optional<Filesystem::Path> file;
    while (!m_stopping.load(std::memory_order_acquire) && (file = next_request_file(folder() + "inbox"))){
        handle_request(*file);
    }
    keep_awake();
    m_runs.sync(RunRecordStore::instance());
    write_state();
}

void LabChannel::handle_request(const Filesystem::Path& file){
    const std::string name = file.stem().string();
    std::string text;
    if (!file_to_string(file.string(), text)){
        global_logger_tagged().log("Lab: cannot read " + file.string(), COLOR_RED);
    }
    Filesystem::rename(file, Filesystem::Path(folder() + "done/" + file.filename().string()));
    global_logger_tagged().log("Lab: running " + name);

    const LabRequest request = parse_lab_request(text);
    std::ostringstream out;
    LabCommandOutput output{out, folder() + "outbox/", name, 0};
    const Filesystem::Path out_path(output.outbox + name + ".out");
    const Filesystem::Path tmp_path(output.outbox + name + ".tmp");

    const WallClock start = current_time();
    std::string error;
    for (const LabRequestLine& line : request.lines){
        const WallClock line_start = current_time();
        output.line = line.number;
        const size_t journal_before = out.str().size();
        error = run_line(request, line, output);
        record_probe(line, out.str().substr(journal_before), error);
        if (!error.empty()){
            out << journal_error(line.number, line.text, error);
            break;
        }
        out << journal_ok(
            line.number, line.text,
            std::chrono::duration_cast<Milliseconds>(line_start - start),
            std::chrono::duration_cast<Milliseconds>(current_time() - start)
        );
        write_lab_file(tmp_path, out.str());
    }
    out << "end\n";
    write_lab_file(tmp_path, out.str());
    Filesystem::remove(out_path);
    Filesystem::rename(tmp_path, out_path);

    m_state.seq = name;
    m_state.error = error;
    m_written_state.clear();
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
        output.out <<
            "channel:\n"
            "  owner claim TOKEN | owner release   (then start with a line \"owner TOKEN\")\n"
            "  select GAME:PROGRAM\n"
            "  set OPTION_KEY JSON\n"
            "  start | stop\n"
            "  state\n"
            "  keepawake on|off [seconds]\n"
            "  help\n";
        m_console->help(output.out);
        return "";
    }
    if (command == "state"){
        write_state();
        output.out << m_written_state;
        return "";
    }
    const LabConsoleCommands::Kind kind = m_console->kind(command);
    if (kind != LabConsoleCommands::Kind::READS_STREAM){
        std::string error = m_owner.check(request.owner);
        if (!error.empty()){
            return error;
        }
    }
    if (command == "select" || command == "set" || command == "start" || command == "stop"){
        return run_ui_command(line, output.out);
    }
    if (command == "keepawake"){
        return m_keep_awake.set(words, current_time());
    }
    if (kind == LabConsoleCommands::Kind::UNKNOWN){
        return "Unknown command: " + command + " (try \"help\")";
    }
    return run_console_command(words, output);
}

//  A screen read (shot, detect, ...) taken during a run, or right after one,
//  goes into that run's folder too, so a probe of a failure sits beside the
//  failure.
void LabChannel::record_probe(const LabRequestLine& line, const std::string& output, const std::string& error){
    if (m_console->kind(line.words[0]) != LabConsoleCommands::Kind::READS_STREAM){
        return;
    }
    m_runs.sync(RunRecordStore::instance());
    const std::optional<std::string> run = m_runs.run_for_probe(current_time());
    if (!run){
        return;
    }
    m_runs.attach_probe(*run, line.text, error.empty() ? output : output + "error: " + error + "\n");
}

std::string LabChannel::run_console_command(const std::vector<std::string>& words, LabCommandOutput& output){
    const bool reads_only = m_console->kind(words[0]) == LabConsoleCommands::Kind::READS_STREAM;
    return run_on_console([&](PanelSession& panel, CancellableScope& scope){
        const ProgramSession* program = dynamic_cast<const ProgramSession*>(&panel);
        if (program == nullptr){
            throw ParseException("The selected panel is not a program.");
        }
        const ProgramState state = program->current_state();
        if (!reads_only && state != ProgramState::STOPPED){
            throw ParseException(
                "Program is " + program_state_name(state) + "; only commands that read the screen run now."
            );
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

std::string LabChannel::run_ui_command(const LabRequestLine& line, std::ostream& out){
    const std::vector<std::string>& words = line.words;
    const std::string& command = words[0];
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
    const bool start = command == "start";
    std::string run_id;
    const std::string error = run_on_ui([&]{
        PanelSession* panel = m_callbacks.current_panel();
        std::string error = start_or_stop(panel, start);
        if (error.empty() && start){
            run_id = dynamic_cast<ProgramSession*>(panel)->last_run_id();
        }
        return error;
    });
    if (error.empty() && start){
        //  The run's ID, for Lab/runs/<run_id>/ and "labctl.sh wait".
        out << "run=" << run_id << "\n";
    }
    return error;
}

//  Runs `body` on the UI thread and returns what it returns: "" or an error.
//  Gives up with an error if the app is closing, since the UI thread would
//  never run it.
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
        running = read_program_state().running;
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

LabChannel::SelectedProgram LabChannel::read_program_state() const{
    const ProgramSession* session = dynamic_cast<const ProgramSession*>(m_panel);
    SelectedProgram selected;
    selected.program = m_panel == nullptr ? "" : m_panel->descriptor().identifier();
    if (session == nullptr){
        selected.state = "NO_PROGRAM";
        return selected;
    }
    const ProgramState current = session->current_state();
    selected.state = program_state_name(current);
    selected.run = session->last_run_id();
    selected.running = current != ProgramState::STOPPED;
    return selected;
}

void LabChannel::write_state(){
    {
        std::lock_guard<Mutex> lg(m_panel_lock);
        const SelectedProgram selected = read_program_state();
        m_state.state = selected.state;
        m_state.program = selected.program;
        m_state.run = selected.run;
    }
    m_state.owner = m_owner.token();
    std::string text = lab_state_text(m_state);
    const Filesystem::Path path(folder() + "state");
    if (text != m_written_state || !Filesystem::exists(path)){
        write_lab_file_atomically(path, text);
        m_written_state = std::move(text);
    }
}



}
