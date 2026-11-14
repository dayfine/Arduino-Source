/*  Lab Transition
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the last start or stop of the selected program came from, for
 *  "Lab/state.json": the window, a lab request (with the lab server's id
 *  for it), or the program itself. Nothing here touches the app, so all of
 *  it is unit tested.
 *
 */

#ifndef PokemonAutomation_LabTransition_H
#define PokemonAutomation_LabTransition_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "CommonFramework/Globals.h"

namespace PokemonAutomation{


//  A new random (version 4) UUID, lower case: "d8ea6a9d-1eab-4a40-b355-d88072b289a1".
std::string new_lab_uuid();

//  Whether `text` is a lower-case UUID, as the lab server writes them.
bool is_lab_uuid(std::string_view text);


//  Whether a transition started or stopped the program.
enum class LabTransitionAction{
    START,
    STOP,
};

//  Where a start or stop came from.
enum class LabTransitionSource{
    UI,             //  The Start/Stop button (or anything else outside the lab channel).
    LAB,            //  A lab request.
    PROGRAM,        //  The program finished by itself.
    PROGRAM_ERROR,  //  The program ended with an error.
};

//  One start or stop of the selected program.
struct LabTransition{
    LabTransitionAction action;
    LabTransitionSource source;
    std::optional<std::string> request_id;  //  The lab request's id; only for LAB.
    WallClock at;
};


//  Names the source of each start and stop of the selected program from its
//  state changes. The lab channel calls expect_lab() just before it starts or
//  stops the program itself; any other start or stop came from the window.
//  Thread-safe: the state changes arrive on the thread that made them.
class LabTransitionTracker{
public:
    void expect_lab(LabTransitionAction action, std::string request_id);
    //  The lab's start or stop was refused, so no state change follows.
    void cancel_lab();

    //  The program's ProgramSession::Listener calls.
    void on_state_change(ProgramState state, WallClock now);
    void on_error();

    //  A different program is selected: its state starts over.
    void reset_program(ProgramState state);

    std::optional<LabTransition> last() const;

private:
    struct Expected{
        LabTransitionAction action;
        std::string request_id;
    };
    LabTransitionSource source_of(LabTransitionAction action, std::optional<std::string>& request_id);

    mutable Mutex m_lock;
    ProgramState m_state = ProgramState::STOPPED;
    bool m_error = false;
    std::optional<Expected> m_expected;
    std::optional<LabTransition> m_last;
};



}  // namespace PokemonAutomation
#endif
