/*  Lab Hold
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The user's hold on the console. While "Lab/hold" names a holder, the lab
 *  channel refuses every command that presses, starts, stops or changes the
 *  program; commands that only read still run. The user's own driving puts
 *  it on: a Start or Stop clicked in the window, or a press from the
 *  window's controller input (keyboard or gamepad). Nothing takes it off by
 *  itself: only "hold.sh off" or the lab server's "lease release-user"
 *  removes the file. It is a file so that it outlives an app restart and
 *  the lab scripts read it without the app. Nothing here touches the app,
 *  so all of it is unit tested.
 *
 */

#ifndef PokemonAutomation_LabHold_H
#define PokemonAutomation_LabHold_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ProgramTransition.h"

namespace PokemonAutomation{


struct LabHold{
    std::string owner;          //  "user"
    //  What put it on: "ui_start", "ui_stop", "user_input" (the app), "hold_sh".
    std::string reason;
    std::string acquired_at;    //  As the writer wrote it, UTC.

    bool operator==(const LabHold& other) const = default;
};

//  Lab/hold's text: "owner=", "reason=" and "acquired_at=" lines.
std::string lab_hold_text(const LabHold& hold);

//  The hold that Lab/hold's `text` names, or nullopt when it names no owner.
//  Unknown lines are skipped.
std::optional<LabHold> parse_lab_hold(std::string_view text);

//  Whether the channel command `words` still runs while the console is held:
//  only those that read (help, state, owner, snapshot, describe_program, a
//  console command that only reads the stream) and "keepawake off".
bool lab_runs_while_held(const std::vector<std::string>& words, bool reads_stream);

//  The journal's error for a command the hold refuses.
std::string lab_hold_refusal(const LabHold& hold);


//  Notices the user driving the selected program. Each Start, Stop or press
//  is reported once, so a hold taken off stays off until the user drives
//  again.
class LabHoldTrigger{
public:
    //  Why a hold goes on now, or nullopt: "ui_start" or "ui_stop" for a new
    //  Start or Stop from the window in `transition`, "user_input" for a
    //  press from the window newer than the last one seen. Anything from
    //  before `selected_at`, when the panel was selected, is not news.
    std::optional<std::string> check(
        const std::optional<ProgramTransition>& transition,
        std::optional<WallClock> last_user_input,
        WallClock selected_at
    );

private:
    std::optional<WallClock> m_transition_seen;
    std::optional<WallClock> m_input_seen;
};



}  // namespace PokemonAutomation
#endif
