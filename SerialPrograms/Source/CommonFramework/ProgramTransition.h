/*  Program Transition
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Who started or stopped a program: the Start/Stop button, a lab request
 *  (with the lab server's id for it), or the program itself when it ends.
 *  ProgramSession passes the asker in with each start and stop and keeps
 *  the last transition here, so a reader such as the lab channel never has
 *  to guess it from the state changes. Free of Qt and of ProgramSession, so
 *  it is unit tested.
 *
 */

#ifndef PokemonAutomation_CommonFramework_ProgramTransition_H
#define PokemonAutomation_CommonFramework_ProgramTransition_H

#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Concurrency/Mutex.h"

namespace PokemonAutomation{


//  Whether a transition started or stopped the program.
enum class ProgramAction{
    START,
    STOP,
};

//  Who asked for a start or stop.
enum class ActionSource{
    UI,             //  The Start/Stop button, or an integration (e.g. Discord).
    LAB,            //  A lab channel request.
    PROGRAM,        //  The program finished by itself.
    PROGRAM_ERROR,  //  The program ended with an error.
};

//  One start or stop of a program.
struct ProgramTransition{
    ProgramAction action;
    ActionSource source;
    std::optional<std::string> request_id;  //  The lab request's id, if a lab request asked.
    WallClock at;
};


//  The last transition of one program. A stop keeps its asker from STOPPING
//  through STOPPED: an error or the program's own end after a stop request
//  does not overwrite who stopped it. Thread-safe.
class ProgramTransitionLatch{
public:
    //  The program went RUNNING because `source` asked.
    void started(ActionSource source, std::optional<std::string> request_id, WallClock now);
    //  The program went STOPPING because `source` asked.
    void stop_requested(ActionSource source, std::optional<std::string> request_id, WallClock now);
    //  The running program reported an error.
    void error_reported();
    //  The program went STOPPED.
    void finished(WallClock now);

    std::optional<ProgramTransition> last() const;

private:
    mutable Mutex m_lock;
    bool m_stop_requested = false;
    bool m_error = false;
    std::optional<ProgramTransition> m_last;
};



}  // namespace PokemonAutomation
#endif
