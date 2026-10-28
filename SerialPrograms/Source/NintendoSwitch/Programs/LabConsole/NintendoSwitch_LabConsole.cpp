/*  Lab Console
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_StartGameUserSelectDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "NintendoSwitch_LabConsole.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

using namespace std::chrono_literals;


namespace{

struct Registry{
    std::mutex lock;
    std::map<std::string, LabConsoleDetectorFactory> detectors;
    std::map<std::string, std::pair<std::string, LabConsoleCommand>> commands;
};
Registry& registry(){
    static Registry registry;
    return registry;
}

//  Detectors every game can use.
bool register_builtin_detectors(){
    register_lab_console_detector("keyboard", [](ConsoleHandle&){ return std::make_unique<SystemKeyboardDetector>(); });
    register_lab_console_detector("black", [](ConsoleHandle&){ return std::make_unique<BlackScreenDetector>(); });
    register_lab_console_detector("white", [](ConsoleHandle&){ return std::make_unique<WhiteScreenDetector>(); });
    register_lab_console_detector("home", [](ConsoleHandle& console){ return std::make_unique<HomeMenuDetector>(console, COLOR_RED); });
    register_lab_console_detector("user_select", [](ConsoleHandle& console){ return std::make_unique<StartGameUserSelectDetector>(console); });
    return true;
}

}

void register_lab_console_detector(const std::string& name, LabConsoleDetectorFactory factory){
    Registry& r = registry();
    std::lock_guard<std::mutex> lg(r.lock);
    r.detectors[name] = std::move(factory);
}
void register_lab_console_command(const std::string& name, const std::string& usage, LabConsoleCommand command){
    Registry& r = registry();
    std::lock_guard<std::mutex> lg(r.lock);
    r.commands[name] = {usage, std::move(command)};
}



LabConsole_Descriptor::LabConsole_Descriptor()
    : SingleSwitchProgramDescriptor(
        "NintendoSwitch:LabConsole",
        "Nintendo Switch", "Lab Console",
        "",
        "Developer tool: run command files from <profile>/LabConsole/inbox, one line at a time, "
        "with a timed log and screenshots in <profile>/LabConsole/outbox.",
        PanelQualityStanding::DEVELOPER,
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}
LabConsole::LabConsole(){}



namespace{

std::vector<std::string> split_words(const std::string& line){
    std::vector<std::string> words;
    std::istringstream in(line);
    std::string word;
    while (in >> word){
        if (word[0] == '#'){
            break;
        }
        words.push_back(word);
    }
    return words;
}

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


class CommandRunner{
public:
    CommandRunner(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::string out_prefix)
        : m_env(env)
        , m_context(context)
        , m_out_prefix(std::move(out_prefix))
    {}

    //  Runs one line. Returns false for "stop". Throws to fail the line.
    bool run(const std::vector<std::string>& w, std::ostream& out){
        const std::string& cmd = w[0];
        auto num = [&](size_t i, double fallback){ return i < w.size() ? std::stod(w[i]) : fallback; };
        auto ms = [&](size_t i, double fallback){ return Milliseconds((int64_t)num(i, fallback)); };
        auto arg = [&](size_t i) -> const std::string& {
            if (i >= w.size()){
                throw ParseException(cmd + ": missing argument " + std::to_string(i));
            }
            return w[i];
        };
        ProControllerContext& context = m_context;

        if (cmd == "stop"){
            return false;
        }else if (cmd == "press"){                  //  press BTN[+BTN] [hold] [release]
            pbf_press_button(context, parse_buttons(arg(1)), ms(2, 160), ms(3, 160));
        }else if (cmd == "dpad"){                   //  dpad UP|DOWN|LEFT|RIGHT [hold] [release]
            pbf_press_dpad(context, parse_dpad(arg(1)), ms(2, 100), ms(3, 200));
        }else if (cmd == "stick"){                  //  stick L|R x y hold [release]
            const JoystickPosition position{num(2, 0), num(3, 0)};
            if (arg(1) == "R"){
                pbf_move_right_joystick(context, position, ms(4, 160), ms(5, 160));
            }else{
                pbf_move_left_joystick(context, position, ms(4, 160), ms(5, 160));
            }
        }else if (cmd == "wait"){                   //  wait ms
            pbf_wait(context, ms(1, 0));
        }else if (cmd == "shot"){                   //  shot label
            out << "shot " << save_shot(arg(1)) << "\n";
        }else if (cmd == "shots"){                  //  shots label count interval_ms
            const int count = (int)num(2, 1);
            for (int c = 0; c < count; c++){
                if (c > 0){
                    context.wait_for(ms(3, 500));
                }
                out << "shot " << save_shot(arg(1) + "_" + std::to_string(c)) << "\n";
            }
        }else if (cmd == "detect"){                 //  detect name [image]: one frame, prints yes/no
            const bool found = w.size() > 2
                ? make_detector(arg(1))->detect(load_image(w[2]))
                : detect_once(arg(1));
            out << "detect " << arg(1) << " " << (found ? "yes" : "no") << "\n";
        }else if (cmd == "wait_for" || cmd == "expect"){
            //  wait_for name timeout_ms: report when it shows up or that it didn't.
            //  expect name timeout_ms: the same, but fail the line if it doesn't.
            const auto found = wait_for_detector(arg(1), ms(2, 5000));
            if (found){
                out << cmd << " " << w[1] << " after " << found->count() << "ms\n";
            }else if (cmd == "expect"){
                throw OperationFailedException(
                    ErrorReportMode::NO_ERROR_REPORT,
                    "expect " + w[1] + ": not seen within " + std::to_string(ms(2, 5000).count()) + "ms."
                );
            }else{
                out << cmd << " " << w[1] << " timeout\n";
            }
        }else if (cmd == "state"){                  //  state [image]: every detector on one frame
            out << "state " << (w.size() > 1 ? state_of(load_image(w[1])) : state()) << "\n";
        }else if (cmd == "cursor"){                 //  cursor: the system keyboard's cursor key
            context.wait_for_all_requests();
            VideoSnapshot screen = m_env.console.video().snapshot();
            int row = -1;
            int col = -1;
            if (screen && find_system_keyboard_cursor(screen, row, col)){
                out << "cursor row " << row << " col " << col << "\n";
            }else{
                out << "cursor none\n";
            }
        }else if (cmd == "close_game"){             //  close_game: from anywhere to HOME with the game closed
            go_home(m_env.console, context);
            close_game_from_home(m_env.console, context);
        }else if (cmd == "type"){                   //  type text: on the system keyboard
            type_on_system_keyboard(m_env.console, context, arg(1));
        }else if (cmd == "help"){
            out << help();
        }else{
            LabConsoleCommand command;
            {
                Registry& r = registry();
                std::lock_guard<std::mutex> lg(r.lock);
                auto iter = r.commands.find(cmd);
                if (iter == r.commands.end()){
                    throw ParseException("Unknown command: " + cmd + " (try \"help\")");
                }
                command = iter->second.second;
            }
            command(m_env, context, w, out);
        }
        return true;
    }

    std::string save_shot(const std::string& label){
        m_context.wait_for_all_requests();
        VideoSnapshot screen = m_env.console.video().snapshot();
        if (!screen){
            return "(no video frame)";
        }
        const std::string path = m_out_prefix + "_" + label + ".png";
        screen.frame->save(path);
        return path;
    }

private:
    //  An image saved earlier, to test detectors offline: an absolute path, or a file
    //  name in the outbox (e.g. one a "shot" line saved).
    ImageRGB32 load_image(const std::string& file) const{
        const std::string path = file[0] == '/'
            ? file
            : m_out_prefix.substr(0, m_out_prefix.rfind('/') + 1) + file;
        ImageRGB32 image(path);
        if (!image){
            throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, "Cannot read image: " + path);
        }
        return image;
    }
    std::unique_ptr<StaticScreenDetector> make_detector(const std::string& name){
        Registry& r = registry();
        std::lock_guard<std::mutex> lg(r.lock);
        auto iter = r.detectors.find(name);
        if (iter == r.detectors.end()){
            throw ParseException("Unknown detector: " + name + " (try \"help\")");
        }
        return iter->second(m_env.console);
    }
    bool detect_once(const std::string& name){
        std::unique_ptr<StaticScreenDetector> detector = make_detector(name);
        m_context.wait_for_all_requests();
        VideoSnapshot screen = m_env.console.video().snapshot();
        return screen && detector->detect(screen);
    }
    std::optional<Milliseconds> wait_for_detector(const std::string& name, Milliseconds timeout){
        std::unique_ptr<StaticScreenDetector> detector = make_detector(name);
        m_context.wait_for_all_requests();
        const WallClock start = current_time();
        while (current_time() - start < timeout){
            VideoSnapshot screen = m_env.console.video().snapshot();
            if (screen && detector->detect(screen)){
                return std::chrono::duration_cast<Milliseconds>(current_time() - start);
            }
            m_context.wait_for(100ms);
        }
        return std::nullopt;
    }
    std::string state(){
        m_context.wait_for_all_requests();
        VideoSnapshot screen = m_env.console.video().snapshot();
        if (!screen){
            return "(no video frame)";
        }
        return state_of(screen);
    }
    std::string state_of(const ImageViewRGB32& screen){
        std::string found;
        Registry& r = registry();
        std::lock_guard<std::mutex> lg(r.lock);
        for (const auto& item : r.detectors){
            if (item.second(m_env.console)->detect(screen)){
                found += (found.empty() ? "" : " ") + item.first;
            }
        }
        return found.empty() ? "(none)" : found;
    }
    static std::string help(){
        std::string text =
            "commands: stop | press BTN[+BTN] [hold] [release] | dpad DIR [hold] [release]\n"
            "  stick L|R x y hold [release] | wait ms | shot label | shots label count interval_ms\n"
            "  detect name [image] | wait_for name timeout_ms | expect name timeout_ms | state [image] | cursor | type text | close_game\n";
        Registry& r = registry();
        std::lock_guard<std::mutex> lg(r.lock);
        for (const auto& item : r.commands){
            text += "  " + item.second.first + "\n";
        }
        text += "detectors:";
        for (const auto& item : r.detectors){
            text += " " + item.first;
        }
        return text + "\n";
    }

    SingleSwitchProgramEnvironment& m_env;
    ProControllerContext& m_context;
    std::string m_out_prefix;
};


void write_file(const std::string& path, const std::string& text){
    FileIO file(Filesystem::Path(path), FileMode::WRITE);
    if (file.is_open()){
        file.write(text.data(), text.size());
    }
}

}



void LabConsole::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    static const bool builtins = register_builtin_detectors();
    (void)builtins;

    const std::string root = USER_FILE_PATH() + "LabConsole/";
    for (const char* folder : {"inbox", "outbox", "done"}){
        Filesystem::create_directories(Filesystem::Path(root + folder));
    }
    env.log("Lab Console: waiting for command files in " + root + "inbox");

    while (true){
        //  The oldest .txt file in the inbox runs next.
        std::vector<Filesystem::Path> files;
        for (const Filesystem::Path& path : Filesystem::list_directory(Filesystem::Path(root + "inbox"))){
            if (Filesystem::is_regular_file(path) && path.extension().string() == ".txt"){
                files.push_back(path);
            }
        }
        if (files.empty()){
            context.wait_for(500ms);
            continue;
        }
        std::sort(files.begin(), files.end(), [](const Filesystem::Path& a, const Filesystem::Path& b){
            return a.string() < b.string();
        });
        const Filesystem::Path file = files.front();
        const std::string name = file.stem().string();
        const std::string out_prefix = root + "outbox/" + name;
        std::string script;
        if (!file_to_string(file.string(), script)){
            env.log("Lab Console: cannot read " + file.string(), COLOR_RED);
        }
        Filesystem::rename(file, Filesystem::Path(root + "done/" + file.filename().string()));
        env.log("Lab Console: running " + name);

        //  Each line ends "ok" with its timing, or the file stops with "error" and a
        //  screenshot of the moment it failed. The last line is always "end".
        std::ostringstream out;
        CommandRunner runner(env, context, out_prefix);
        std::istringstream lines(script);
        std::string line;
        size_t line_number = 0;
        bool keep_going = true;
        const WallClock file_start = current_time();
        while (keep_going && std::getline(lines, line)){
            line_number++;
            if (!line.empty() && line.back() == '\r'){
                line.pop_back();
            }
            const std::vector<std::string> words = split_words(line);
            if (words.empty()){
                continue;
            }
            const WallClock start = current_time();
            std::string error;
            try{
                keep_going = runner.run(words, out);
                context.wait_for_all_requests();
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
                out << "error line " << line_number << " (" << line << "): " << error << "\n";
                out << "shot " << runner.save_shot("error_L" + std::to_string(line_number)) << "\n";
                break;
            }
            const auto t0 = std::chrono::duration_cast<Milliseconds>(start - file_start).count();
            const auto t1 = std::chrono::duration_cast<Milliseconds>(current_time() - file_start).count();
            out << "ok " << line_number << " " << line << " @" << t0 << "-" << t1 << "ms\n";
            write_file(out_prefix + ".tmp", out.str());
        }
        out << "end\n";
        write_file(out_prefix + ".tmp", out.str());
        Filesystem::remove(Filesystem::Path(out_prefix + ".out"));
        Filesystem::rename(Filesystem::Path(out_prefix + ".tmp"), Filesystem::Path(out_prefix + ".out"));
        if (!keep_going){
            env.log("Lab Console: stop.");
            return;
        }
    }
}



}
}
