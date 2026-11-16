/*  Lab Hold
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "LabHold.h"

namespace PokemonAutomation{


std::string lab_hold_text(const LabHold& hold){
    return "owner=" + hold.owner + "\n"
        + "reason=" + hold.reason + "\n"
        + "acquired_at=" + hold.acquired_at + "\n";
}

std::optional<LabHold> parse_lab_hold(std::string_view text){
    LabHold hold;
    while (!text.empty()){
        const size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
        if (!line.empty() && line.back() == '\r'){
            line.remove_suffix(1);
        }
        const size_t equals = line.find('=');
        if (equals == std::string_view::npos){
            continue;
        }
        const std::string_view key = line.substr(0, equals);
        const std::string value(line.substr(equals + 1));
        if (key == "owner"){
            hold.owner = value;
        }else if (key == "reason"){
            hold.reason = value;
        }else if (key == "acquired_at"){
            hold.acquired_at = value;
        }
    }
    if (hold.owner.empty()){
        return std::nullopt;
    }
    return hold;
}

bool lab_runs_while_held(const std::vector<std::string>& words, bool reads_stream){
    if (reads_stream || words.empty()){
        return true;
    }
    static constexpr std::string_view READS[] = {"help", "state", "owner", "snapshot", "describe_program"};
    const std::string& command = words[0];
    if (std::find(std::begin(READS), std::end(READS), command) != std::end(READS)){
        return true;
    }
    return command == "keepawake" && words.size() == 2 && words[1] == "off";
}

std::string lab_hold_refusal(const LabHold& hold){
    return "console held by " + hold.owner + " since " + hold.acquired_at + " (" + hold.reason
        + "): the user is on the console; ~/BDSPLab/hold.sh off releases it";
}


std::optional<std::string> LabHoldTrigger::check(
    const std::optional<ProgramTransition>& transition,
    std::optional<WallClock> last_user_input,
    WallClock selected_at
){
    std::optional<std::string> reason;
    if (transition && transition->at >= selected_at && transition->at != m_transition_seen){
        m_transition_seen = transition->at;
        if (transition->source == ActionSource::UI){
            reason = transition->action == ProgramAction::START ? "ui_start" : "ui_stop";
        }
    }
    if (last_user_input && *last_user_input >= selected_at
        && (!m_input_seen || *last_user_input > *m_input_seen)
    ){
        m_input_seen = last_user_input;
        if (!reason){
            reason = "user_input";
        }
    }
    return reason;
}



}  // namespace PokemonAutomation
