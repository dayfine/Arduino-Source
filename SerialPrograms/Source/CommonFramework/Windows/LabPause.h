/*  Lab Pause
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The lab's wait for a program's breakpoints (AutoStory's BREAKPOINTS
 *  option): the program stops at a pause point, publishes the point's label
 *  as "paused=<label>" in the Lab channel's "state" file, and goes on when
 *  the file "Lab/continue" appears. The file may hold "continue N" (or just
 *  "N"): the program then goes on and passes the next N pause points
 *  without stopping. Stop Program ends the wait. Lab.md documents it.
 *
 */

#ifndef PokemonAutomation_LabPause_H
#define PokemonAutomation_LabPause_H

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/CancellableScope.h"

namespace PokemonAutomation{


//  How many pause points the program passes after this one, from the text
//  of a "continue" file: "" and "continue" are 0, "continue 3" and "3" are 3.
//  Anything else is 0.
size_t parse_lab_continue(const std::string& text);

//  The files and the clock a pause uses, so it can be tested without either.
struct LabPauseIO{
    //  The "continue" file's text, if the file exists. Removes the file.
    std::function<std::optional<std::string>()> take_continue;
    //  Waits a slice. Throws when the program is cancelled.
    std::function<void()> sleep;
};

//  The pause points the user already told the program to pass.
void lab_pause_set_skips(size_t skips);

//  A pause point: passes at once while skips are left, else publishes
//  `label`, waits for the continue file (`io.sleep` between looks) and takes
//  its skip count. The label is cleared however the wait ends.
void lab_pause(std::string_view label, const LabPauseIO& io);

//  The label the program is paused at, or "" while it runs.
std::string lab_paused_label();

//  lab_pause() on the real "Lab/continue" file, sleeping on `scope`. This is
//  what a lab build installs as the story engine's pause wait.
void lab_pause_wait(std::string_view label, CancellableScope& scope);



}
#endif
