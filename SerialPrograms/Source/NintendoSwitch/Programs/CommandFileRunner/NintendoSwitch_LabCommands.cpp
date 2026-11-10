/*  Switch Lab Commands
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Exceptions.h"
#include "GameConsole/Framework/ConsoleProgramSession.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch_CommandFileRunner.h"
#include "NintendoSwitch_LabCommands.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

using namespace std::chrono_literals;


namespace{

//  The console of a stopped program, with a Pro Controller context for the
//  command's scope.
class LabConsole{
public:
    LabConsole(PanelSession& panel, CancellableScope& scope)
        : m_console(system_of(panel))
        , m_context(scope, m_console.controller<ProController>())
    {
        if (!m_context->is_ready()){
            throw ParseException("Controller is not ready.");
        }
    }

    ConsoleHandle& console(){ return m_console; }
    ProControllerContext& context(){ return m_context; }

private:
    static GameConsole::ConsoleSystemSession& system_of(PanelSession& panel){
        GameConsole::ConsoleProgramSession* program = dynamic_cast<GameConsole::ConsoleProgramSession*>(&panel);
        if (program == nullptr){
            throw ParseException("The selected program has no console of its own.");
        }
        return program->system();
    }

    ConsoleHandle m_console;
    ProControllerContext m_context;
};

}



LabConsoleCommands::Kind SwitchLabCommands::kind(const std::string& command) const{
    switch (command_file_command_kind(command)){
    case CommandFileCommandKind::READS_STREAM:
        return Kind::READS_STREAM;
    case CommandFileCommandKind::DRIVES_CONSOLE:
        return Kind::DRIVES_CONSOLE;
    case CommandFileCommandKind::UNKNOWN:
        break;
    }
    return Kind::UNKNOWN;
}

void SwitchLabCommands::run(
    PanelSession& panel, CancellableScope& scope,
    const std::vector<std::string>& words, LabCommandOutput& output
){
    LabConsole console(panel, scope);
    //  A command that only reads the screen may run while a program drives
    //  the controller; it must not wait for that program's presses.
    const bool drives = command_file_command_kind(words[0]) == CommandFileCommandKind::DRIVES_CONSOLE;
    CommandFileSession session{console.console(), console.context(), output.out, output.outbox, output.request, drives};
    try{
        run_command_file_line(session, words);
        if (drives){
            session.context.wait_for_all_requests();
        }
    }catch (OperationCancelledException&){
        throw;
    }catch (...){
        //  What the screen showed when the command failed.
        output.out << "shot " << save_command_file_screenshot(session, std::format("error_L{}", output.line)) << "\n";
        throw;
    }
}

void SwitchLabCommands::keep_awake(PanelSession& panel, CancellableScope& scope){
    LabConsole console(panel, scope);
    pbf_press_button(console.context(), BUTTON_B, 80ms, 0ms);
    console.context().wait_for_all_requests();
}

void SwitchLabCommands::help(std::ostream& out){
    print_command_file_help(out);
}



}
}
