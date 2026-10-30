/*  Lab Control
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <exception>
#include <vector>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/ProgramSession.h"
#include "CommonFramework/Panels/UI/PanelElements.h"
#include "LabControl.h"

namespace PokemonAutomation{



LabControlAction parse_lab_control_file(const std::string& filename){
    if (filename == "start"){
        return LabControlAction::START;
    }
    if (filename == "stop"){
        return LabControlAction::STOP;
    }
    return LabControlAction::NONE;
}

std::string lab_control_state_text(
    const std::string& state,
    const std::string& program,
    const std::string& error
){
    std::string text = "state=" + state + "\nprogram=" + program + "\n";
    if (!error.empty()){
        text += "error=" + error + "\n";
    }
    return text;
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


namespace{

//  Returns an error message, or empty on success.
std::string run_action(ProgramSession* program, LabControlAction action){
    if (program == nullptr){
        return "The selected panel is not a program.";
    }
    ProgramState state = program->current_state();
    ProgramState from = action == LabControlAction::START
        ? ProgramState::STOPPED
        : ProgramState::RUNNING;
    if (state != from){
        return "Program is " + program_state_name(state) + ".";
    }
    start_or_stop_program(*program);
    return "";
}

void write_state_file(const Filesystem::Path& path, const std::string& text){
    Filesystem::Path temp = Filesystem::unique_temp_path(path);
    {
        FileIO file;
        if (!file.open(temp, FileMode::WRITE | FileMode::BINARY)){
            return;
        }
        file.write(text);
    }
    std::error_code ec;
    Filesystem::rename(temp, path, ec);
}

}


void poll_lab_control(PanelSession* panel, LabControlStatus& status){
    const Filesystem::Path folder(USER_FILE_PATH() + "LabControl");
    if (!Filesystem::is_directory(folder)){
        status.written_state.clear();
        return;
    }

    ProgramSession* program = dynamic_cast<ProgramSession*>(panel);
    try{
        for (const Filesystem::Path& path : Filesystem::list_directory(folder)){
            LabControlAction action = parse_lab_control_file(path.filename().string());
            if (action == LabControlAction::NONE){
                continue;
            }
            Filesystem::remove(path);
            status.last_error = run_action(program, action);
            global_logger_tagged().log(
                "LabControl: " + path.filename().string() +
                (status.last_error.empty() ? "" : " ignored: " + status.last_error)
            );
        }

        std::string text = lab_control_state_text(
            program == nullptr ? "NO_PROGRAM" : program_state_name(program->current_state()),
            panel == nullptr ? "" : panel->descriptor().identifier(),
            status.last_error
        );
        const Filesystem::Path state_path = folder / "state";
        if (text != status.written_state || !Filesystem::exists(state_path)){
            write_state_file(state_path, text);
            status.written_state = std::move(text);
        }
    }catch (const std::exception& e){
        global_logger_tagged().log(std::string("LabControl: ") + e.what(), COLOR_RED);
    }
}



class Test_LabControl_ParseFile : public UnitTest{
public:
    Test_LabControl_ParseFile()
        : UnitTest("CommonFramework::LabControl - parse_lab_control_file()")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (parse_lab_control_file("start") != LabControlAction::START){
            return "\"start\" should start.";
        }
        if (parse_lab_control_file("stop") != LabControlAction::STOP){
            return "\"stop\" should stop.";
        }
        //  Our own output, scripts' partial writes and anything else are ignored.
        for (const char* name : {"state", "start.part", "stop.part", "Start", "start.txt", ""}){
            if (parse_lab_control_file(name) != LabControlAction::NONE){
                return std::string("\"") + name + "\" should be ignored.";
            }
        }
        return true;
    }
};

class Test_LabControl_StateText : public UnitTest{
public:
    Test_LabControl_StateText()
        : UnitTest("CommonFramework::LabControl - lab_control_state_text()")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::string text = lab_control_state_text(
            program_state_name(ProgramState::RUNNING), "PokemonBDSP:AutoStory", ""
        );
        if (text != "state=RUNNING\nprogram=PokemonBDSP:AutoStory\n"){
            return "Unexpected state text: " + text;
        }
        text = lab_control_state_text("NO_PROGRAM", "", "The selected panel is not a program.");
        if (text != "state=NO_PROGRAM\nprogram=\nerror=The selected panel is not a program.\n"){
            return "Unexpected state text: " + text;
        }
        return true;
    }
};

void add_tests_LabControl(UnitTestDatabase& database){
    database.add<Test_LabControl_ParseFile>();
    database.add<Test_LabControl_StateText>();
}



}
