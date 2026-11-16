/*  Lab Control
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Lets test scripts start and stop the selected program without clicking
 *  the window. Only active when the folder "<user files>/LabControl/" exists,
 *  so normal users see no change.
 *
 *  A file named "start" or "stop" in that folder does what clicking
 *  Start/Stop Program does, and is then deleted. The file "state" holds the
 *  current program state and the selected program's identifier.
 *
 */

#ifndef PokemonAutomation_LabControl_H
#define PokemonAutomation_LabControl_H

#include <string>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/Panels/PanelSession.h"

namespace PokemonAutomation{


enum class LabControlAction{
    NONE,
    START,
    STOP,
};

//  The action requested by a file of this name in the control folder.
LabControlAction parse_lab_control_file(const std::string& filename);

//  Contents of the "state" file. "state" is NO_PROGRAM when the selected panel
//  is not a program. "error" is only written after a failed start/stop.
std::string lab_control_state_text(
    const std::string& state,
    const std::string& program,
    const std::string& error
);

std::string program_state_name(ProgramState state);


struct LabControlStatus{
    std::string written_state;  //  Last contents written to "state".
    std::string last_error;     //  Error from the last start/stop, if any.
};

//  Act on any control files and refresh "state". Call from the UI thread.
//  "panel" is the selected panel, or nullptr if there is none.
void poll_lab_control(PanelSession* panel, LabControlStatus& status);

void add_tests_LabControl(UnitTestDatabase& database);



}
#endif
