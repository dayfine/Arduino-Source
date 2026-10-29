/*  Command File Runner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <map>
#include <optional>
#include <sstream>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_StartGameUserSelectDetector.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "NintendoSwitch_CommandFileRunner.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

using namespace std::chrono_literals;


namespace{

//  Defaults for timings a command line leaves out.
constexpr Milliseconds BUTTON_HOLD = 160ms;         //  Long enough for every game to see the press.
constexpr Milliseconds BUTTON_RELEASE = 160ms;
constexpr Milliseconds DPAD_HOLD = 100ms;           //  One menu step without auto-repeat.
constexpr Milliseconds DPAD_RELEASE = 200ms;
constexpr Milliseconds STICK_HOLD = 160ms;
constexpr Milliseconds STICK_RELEASE = 160ms;
constexpr Milliseconds SHOTS_INTERVAL = 500ms;
constexpr Milliseconds WAIT_FOR_TIMEOUT = 5000ms;

//  How often "wait_for" and "expect" check a new frame.
constexpr Milliseconds DETECTOR_POLL_INTERVAL = 100ms;

//  How often an empty inbox is checked for a new file.
constexpr Milliseconds INBOX_POLL_INTERVAL = 500ms;



struct Registry{
    Mutex lock;
    std::map<std::string, CommandFileDetectorFactory> detectors;
    std::map<std::string, std::pair<std::string, CommandFileCommand>> commands;     //  name -> usage, command
};
Registry& registry(){
    static Registry registry;
    return registry;
}

//  Detectors every game can use.
void register_builtin_detectors(){
    static const bool registered = []{
        register_command_file_detector("black", [](ConsoleHandle&){ return std::make_unique<BlackScreenDetector>(); });
        register_command_file_detector("white", [](ConsoleHandle&){ return std::make_unique<WhiteScreenDetector>(); });
        register_command_file_detector("home", [](ConsoleHandle& console){ return std::make_unique<HomeMenuDetector>(console, COLOR_RED); });
        register_command_file_detector("user_select", [](ConsoleHandle& console){ return std::make_unique<StartGameUserSelectDetector>(console); });
        return true;
    }();
    (void)registered;
}

std::unique_ptr<StaticScreenDetector> make_detector(const std::string& name, ConsoleHandle& console){
    Registry& r = registry();
    LockGuard<Mutex> lg(r.lock);
    auto iter = r.detectors.find(name);
    if (iter == r.detectors.end()){
        throw ParseException("Unknown detector: " + name + " (try \"help\")");
    }
    return iter->second(console);
}

std::optional<CommandFileCommand> find_registered_command(const std::string& name){
    Registry& r = registry();
    LockGuard<Mutex> lg(r.lock);
    auto iter = r.commands.find(name);
    if (iter == r.commands.end()){
        return std::nullopt;
    }
    return iter->second.second;
}

}



void register_command_file_command(const std::string& name, const std::string& usage, CommandFileCommand command){
    Registry& r = registry();
    LockGuard<Mutex> lg(r.lock);
    r.commands[name] = {usage, std::move(command)};
}
void register_command_file_detector(const std::string& name, CommandFileDetectorFactory factory){
    Registry& r = registry();
    LockGuard<Mutex> lg(r.lock);
    r.detectors[name] = std::move(factory);
}



namespace{

//  One line of a command file, split into words.
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


Button parse_buttons(const std::string& text){
    static const std::map<std::string, Button> NAMES{
        {"A", BUTTON_A}, {"B", BUTTON_B}, {"X", BUTTON_X}, {"Y", BUTTON_Y},
        {"L", BUTTON_L}, {"R", BUTTON_R}, {"ZL", BUTTON_ZL}, {"ZR", BUTTON_ZR},
        {"PLUS", BUTTON_PLUS}, {"MINUS", BUTTON_MINUS},
        {"LCLICK", BUTTON_LCLICK}, {"RCLICK", BUTTON_RCLICK},
        {"HOME", BUTTON_HOME}, {"CAPTURE", BUTTON_CAPTURE},
    };
    Button buttons = BUTTON_NONE;
    std::istringstream in(text);
    std::string name;
    while (std::getline(in, name, '+')){
        auto iter = NAMES.find(name);
        if (iter == NAMES.end()){
            throw ParseException("Unknown button: " + name);
        }
        buttons = (Button)(buttons | iter->second);
    }
    return buttons;
}

DpadPosition parse_dpad(const std::string& text){
    static const std::map<std::string, DpadPosition> NAMES{
        {"UP", DPAD_UP}, {"DOWN", DPAD_DOWN}, {"LEFT", DPAD_LEFT}, {"RIGHT", DPAD_RIGHT},
    };
    auto iter = NAMES.find(text);
    if (iter == NAMES.end()){
        throw ParseException("Unknown D-pad direction: " + text);
    }
    return iter->second;
}



//  Runs the lines of one command file.
class LineRunner{
public:
    LineRunner(CommandFileSession& session, std::string name)
        : m_session(session)
        , m_name(std::move(name))
    {}

    //  Runs one line. Returns false for "stop". Throws to fail the line.
    bool run(const CommandLine& line){
        if (line.name() == "stop"){
            return false;
        }
        auto iter = builtins().find(line.name());
        if (iter != builtins().end()){
            (this->*iter->second.handler)(line);
            return true;
        }
        std::optional<CommandFileCommand> command = find_registered_command(line.name());
        if (!command){
            throw ParseException("Unknown command: " + line.name() + " (try \"help\")");
        }
        (*command)(m_session, line.words());
        return true;
    }

    //  Saves the current frame as <outbox>/<file name>_<label>.png and returns its path.
    std::string save_screenshot(const std::string& label){
        VideoSnapshot screen = current_frame();
        if (!screen){
            return "(no video frame)";
        }
        const std::string path = m_session.outbox + m_name + "_" + label + ".png";
        screen.frame->save(path);
        return path;
    }

private:
    using Handler = void (LineRunner::*)(const CommandLine&);
    struct Builtin{
        std::string usage;
        Handler handler;
    };
    static const std::map<std::string, Builtin>& builtins(){
        static const std::map<std::string, Builtin> BUILTINS{
            {"press",       {"press BTN[+BTN] [hold_ms] [release_ms]", &LineRunner::press}},
            {"dpad",        {"dpad UP|DOWN|LEFT|RIGHT [hold_ms] [release_ms]", &LineRunner::dpad}},
            {"stick",       {"stick L|R x y [hold_ms] [release_ms]   (x, y from -1 to 1)", &LineRunner::stick}},
            {"wait",        {"wait ms", &LineRunner::wait}},
            {"shot",        {"shot label", &LineRunner::shot}},
            {"shots",       {"shots label count [interval_ms]", &LineRunner::shots}},
            {"detect",      {"detect detector [image]   (image: a path, or a file in the outbox)", &LineRunner::detect}},
            {"wait_for",    {"wait_for detector [timeout_ms]   (reports a timeout)", &LineRunner::wait_for}},
            {"expect",      {"expect detector [timeout_ms]   (fails on a timeout)", &LineRunner::wait_for}},
            {"state",       {"state [image]   (every detector that fires)", &LineRunner::state}},
            {"close_game",  {"close_game   (from anywhere to Home with the game closed)", &LineRunner::close_game}},
            {"help",        {"help", &LineRunner::help}},
        };
        return BUILTINS;
    }

    ProControllerContext& context(){ return m_session.context; }
    ConsoleHandle& console(){ return m_session.env.console; }
    std::ostream& out(){ return m_session.out; }

    VideoSnapshot current_frame(){
        context().wait_for_all_requests();
        return console().video().snapshot();
    }

    ImageRGB32 load_image(const std::string& file) const{
        const std::string path = file[0] == '/' ? file : m_session.outbox + file;
        ImageRGB32 image(path);
        if (!image){
            throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, "Cannot read image: " + path);
        }
        return image;
    }

    void press(const CommandLine& line){
        pbf_press_button(
            context(), parse_buttons(line.arg(1)),
            line.duration(2, BUTTON_HOLD), line.duration(3, BUTTON_RELEASE)
        );
    }
    void dpad(const CommandLine& line){
        pbf_press_dpad(
            context(), parse_dpad(line.arg(1)),
            line.duration(2, DPAD_HOLD), line.duration(3, DPAD_RELEASE)
        );
    }
    void stick(const CommandLine& line){
        const std::string& side = line.arg(1);
        const JoystickPosition position{line.number(2, 0), line.number(3, 0)};
        const Milliseconds hold = line.duration(4, STICK_HOLD);
        const Milliseconds release = line.duration(5, STICK_RELEASE);
        if (side == "L"){
            pbf_move_left_joystick(context(), position, hold, release);
        }else if (side == "R"){
            pbf_move_right_joystick(context(), position, hold, release);
        }else{
            throw ParseException("stick: expected L or R, got " + side);
        }
    }
    void wait(const CommandLine& line){
        pbf_wait(context(), line.duration(1, 0ms));
    }
    void shot(const CommandLine& line){
        out() << "shot " << save_screenshot(line.arg(1)) << "\n";
    }
    void shots(const CommandLine& line){
        const std::string& label = line.arg(1);
        const int count = (int)line.number(2, 1);
        const Milliseconds interval = line.duration(3, SHOTS_INTERVAL);
        for (int c = 0; c < count; c++){
            if (c > 0){
                context().wait_for(interval);
            }
            out() << "shot " << save_screenshot(label + "_" + std::to_string(c)) << "\n";
        }
    }
    void detect(const CommandLine& line){
        const std::string& name = line.arg(1);
        std::unique_ptr<StaticScreenDetector> detector = make_detector(name, console());
        bool found;
        if (line.has(2)){
            found = detector->detect(load_image(line.arg(2)));
        }else{
            VideoSnapshot screen = current_frame();
            found = screen && detector->detect(screen);
        }
        out() << "detect " << name << " " << (found ? "yes" : "no") << "\n";
    }
    //  "wait_for" and "expect": wait for a detector to fire.
    void wait_for(const CommandLine& line){
        const std::string& name = line.arg(1);
        const Milliseconds timeout = line.duration(2, WAIT_FOR_TIMEOUT);
        std::unique_ptr<StaticScreenDetector> detector = make_detector(name, console());
        context().wait_for_all_requests();
        const WallClock start = current_time();
        while (current_time() - start < timeout){
            VideoSnapshot screen = console().video().snapshot();
            if (screen && detector->detect(screen)){
                const auto elapsed = std::chrono::duration_cast<Milliseconds>(current_time() - start);
                out() << line.name() << " " << name << " after " << elapsed.count() << "ms\n";
                return;
            }
            context().wait_for(DETECTOR_POLL_INTERVAL);
        }
        if (line.name() == "expect"){
            throw OperationFailedException(
                ErrorReportMode::NO_ERROR_REPORT,
                "expect " + name + ": not seen within " + std::to_string(timeout.count()) + "ms."
            );
        }
        out() << line.name() << " " << name << " timeout\n";
    }
    void state(const CommandLine& line){
        if (line.has(1)){
            out() << "state " << detectors_firing(load_image(line.arg(1))) << "\n";
            return;
        }
        VideoSnapshot screen = current_frame();
        out() << "state " << (screen ? detectors_firing(screen) : "(no video frame)") << "\n";
    }
    void close_game(const CommandLine&){
        go_home(console(), context());
        close_game_from_home(console(), context());
    }
    void help(const CommandLine&){
        out() << "commands:\n  stop\n";
        for (const auto& item : builtins()){
            out() << "  " << item.second.usage << "\n";
        }
        Registry& r = registry();
        LockGuard<Mutex> lg(r.lock);
        for (const auto& item : r.commands){
            out() << "  " << item.second.first << "\n";
        }
        out() << "detectors:";
        for (const auto& item : r.detectors){
            out() << " " << item.first;
        }
        out() << "\n";
    }

    std::string detectors_firing(const ImageViewRGB32& screen){
        std::string found;
        Registry& r = registry();
        LockGuard<Mutex> lg(r.lock);
        for (const auto& item : r.detectors){
            if (item.second(console())->detect(screen)){
                found += (found.empty() ? "" : " ") + item.first;
            }
        }
        return found.empty() ? "(none)" : found;
    }

    CommandFileSession& m_session;
    std::string m_name;
};



void write_file(const std::string& path, const std::string& text){
    FileIO file(Filesystem::Path(path), FileMode::WRITE);
    if (file.is_open()){
        file.write(text.data(), text.size());
    }
}

//  The inbox file whose name sorts first, if any.
std::optional<Filesystem::Path> next_command_file(const std::string& inbox){
    std::optional<Filesystem::Path> next;
    for (const Filesystem::Path& path : Filesystem::list_directory(Filesystem::Path(inbox))){
        if (!Filesystem::is_regular_file(path) || path.extension().string() != ".txt"){
            continue;
        }
        if (!next || path.string() < next->string()){
            next = path;
        }
    }
    return next;
}

enum class LineResult{
    NEXT_LINE,
    FAILED,         //  Stop this file.
    STOP,           //  "stop": stop this file and the program.
};

//  Runs one line and adds its "ok" or "error" result to `session.out`.
LineResult run_line(LineRunner& runner, CommandFileSession& session, const std::string& text, size_t number, WallClock file_start){
    const CommandLine line(text);
    if (line.empty()){
        return LineResult::NEXT_LINE;
    }
    const WallClock start = current_time();
    bool keep_going = true;
    std::string error;
    try{
        keep_going = runner.run(line);
        session.context.wait_for_all_requests();
    }catch (ProgramCancelledException&){
        throw;
    }catch (OperationCancelledException&){
        throw;
    }catch (Exception& e){
        error = e.message();
    }catch (std::exception& e){
        error = e.what();
    }
    if (!error.empty()){
        session.out << "error line " << number << " (" << text << "): " << error << "\n";
        session.out << "shot " << runner.save_screenshot("error_L" + std::to_string(number)) << "\n";
        return LineResult::FAILED;
    }
    const auto t0 = std::chrono::duration_cast<Milliseconds>(start - file_start).count();
    const auto t1 = std::chrono::duration_cast<Milliseconds>(current_time() - file_start).count();
    session.out << "ok " << number << " " << text << " @" << t0 << "-" << t1 << "ms\n";
    return keep_going ? LineResult::NEXT_LINE : LineResult::STOP;
}

//  Runs a file from the inbox and moves it to "done". Returns false if it ran "stop".
bool run_command_file(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& folder, const Filesystem::Path& file
){
    const std::string name = file.stem().string();
    std::string script;
    if (!file_to_string(file.string(), script)){
        env.log("Command File Runner: cannot read " + file.string(), COLOR_RED);
    }
    Filesystem::rename(file, Filesystem::Path(folder + "done/" + file.filename().string()));
    env.log("Command File Runner: running " + name);

    std::ostringstream out;
    CommandFileSession session{env, context, out, folder + "outbox/"};
    LineRunner runner(session, name);
    const std::string out_path = session.outbox + name;

    std::istringstream lines(script);
    std::string text;
    size_t number = 0;
    LineResult result = LineResult::NEXT_LINE;
    const WallClock file_start = current_time();
    while (result == LineResult::NEXT_LINE && std::getline(lines, text)){
        number++;
        if (!text.empty() && text.back() == '\r'){
            text.pop_back();
        }
        result = run_line(runner, session, text, number, file_start);
        write_file(out_path + ".tmp", out.str());
    }
    out << "end\n";
    write_file(out_path + ".tmp", out.str());
    Filesystem::remove(Filesystem::Path(out_path + ".out"));
    Filesystem::rename(Filesystem::Path(out_path + ".tmp"), Filesystem::Path(out_path + ".out"));
    return result != LineResult::STOP;
}

}



CommandFileRunner_Descriptor::CommandFileRunner_Descriptor()
    : SingleSwitchProgramDescriptor(
        "NintendoSwitch:CommandFileRunner",
        "Nintendo Switch", "Command File Runner",
        "",
        "Developer tool: control the console from text command files (press buttons, "
        "take screenshots, check detectors) to test detectors and routines on live video.",
        PanelQualityStanding::DEVELOPER,
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}
CommandFileRunner::CommandFileRunner()
    : DESCRIPTION(
        "Put command files (.txt, one command per line) in the inbox folder. Each runs once, "
        "then moves to \"done\"; its results and screenshots go to \"outbox\". "
        "A file containing \"help\" lists every command and detector."
    )
    , FOLDER(
        false,
        "<b>Command Folder:</b><br>Holds the inbox, outbox and done folders. "
        "Leave empty to use \"CommandFiles\" in the settings folder.",
        LockMode::LOCK_WHILE_RUNNING,
        "",
        "CommandFiles"
    )
    , STOP_WHEN_INBOX_EMPTY(
        "<b>Stop When Inbox Is Empty:</b><br>Run the files already in the inbox, then stop, "
        "instead of waiting for more.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
{
    PA_ADD_STATIC(DESCRIPTION);
    PA_ADD_OPTION(FOLDER);
    PA_ADD_OPTION(STOP_WHEN_INBOX_EMPTY);
}

std::string CommandFileRunner::folder() const{
    std::string folder = FOLDER;
    if (folder.empty()){
        folder = USER_FILE_PATH() + "CommandFiles";
    }
    if (folder.back() != '/'){
        folder += '/';
    }
    return folder;
}

void CommandFileRunner::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    register_builtin_detectors();

    const std::string root = folder();
    for (const char* sub : {"inbox", "outbox", "done"}){
        Filesystem::create_directories(Filesystem::Path(root + sub));
    }
    env.log("Command File Runner: waiting for command files in " + root + "inbox");

    while (true){
        std::optional<Filesystem::Path> file = next_command_file(root + "inbox");
        if (!file){
            if (STOP_WHEN_INBOX_EMPTY){
                env.log("Command File Runner: inbox is empty.");
                return;
            }
            context.wait_for(INBOX_POLL_INTERVAL);
            continue;
        }
        if (!run_command_file(env, context, root, *file)){
            env.log("Command File Runner: stop.");
            return;
        }
    }
}



}
}
